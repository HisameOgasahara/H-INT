#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * Vehicle System Practice - Function A: Turn signals
 *
 * Easy Module Shield V1 -> ShieldBuddy TC275 mapping
 * ---------------------------------------------------
 * Potentiometer A0 : ShieldBuddy ADCL.1 -> TC275 SAR4.7 / P32.3
 * SW1 D2           : ShieldBuddy PWML.3 -> TC275 P02.0
 * SW2 D3           : ShieldBuddy PWML.4 -> TC275 P02.1
 * Buzzer D5        : ShieldBuddy PWML.6 -> TC275 P02.3
 * Red LED D12      : ShieldBuddy PWMH.5 -> TC275 P10.1
 * Blue LED D13     : ShieldBuddy PWMH.6 -> TC275 P10.2
 *
 * Behaviour required by the final MCU project slide:
 *   - SW1 selects the left indicator (Blue LED).
 *   - SW2 selects the right indicator (Red LED).
 *   - The selected indicator blinks.
 *   - After steering toward the selected side and returning to centre,
 *     the indicator is cancelled.
 *   - Steering to the opposite side while an indicator is active sounds
 *     the active buzzer as a warning.
 *
 * The A0 direction used here is the usual potentiometer convention:
 * lower ADC value = left, higher ADC value = right.
 * If the actual shield is mechanically reversed, change POT_REVERSED to 1.
 */

/* ============================ PORTS ============================ */
#define PORT2_BASE_ADDRESS         (0xF003A200U)
#define REG_PORT2_IOCR0            (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10U))
#define REG_PORT2_OMR              (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x04U))
#define REG_PORT2_INPUT            (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24U))

#define PORT10_BASE_ADDRESS        (0xF003B000U)
#define REG_PORT10_IOCR0           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10U))
#define REG_PORT10_OMR             (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04U))

/* IOCR field positions: PC0=[7:3], PC1=[15:11], PC2=[23:19], PC3=[31:27]. */
#define PC0_POS                    3U
#define PC1_POS                    11U
#define PC2_POS                    19U
#define PC3_POS                    27U

/* Pin numbers used by IN/OMR. */
#define SW1_PIN                    0U
#define SW2_PIN                    1U
#define BUZZER_PIN                 3U
#define RIGHT_LED_PIN              1U
#define LEFT_LED_PIN               2U

/* ============================= STM ============================= */
#define STM0_BASE_ADDRESS          (0xF0000000U)
#define REG_STM0_TIM0              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))

/* Existing course timer practice uses STM0 at 100 MHz. */
#define STM0_FREQUENCY_HZ          (100000000UL)
#define BLINK_HALF_PERIOD_TICKS    (STM0_FREQUENCY_HZ / 2UL) /* 500 ms */

/* ============================= VADC ============================ */
#define VADC_BASE_ADDRESS          (0xF0020000U)

#define REG_VADC_CLC               (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x000U))
#define REG_VADC_G4ARBCFG          (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1480U))
#define REG_VADC_G4ARBPR           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1484U))
#define REG_VADC_G4ICLASS0         (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x14A0U))
#define REG_VADC_G4QMR0            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1504U))
#define REG_VADC_G4QINR0           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1510U))
#define REG_VADC_G4CHCTR7          (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x161CU))
#define REG_VADC_G4RES1            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1704U))

#define VADC_DISR                  0U
#define VADC_DISS                  1U
#define VADC_ANONC                 0U
#define VADC_PRIO0                 0U
#define VADC_CSM0                  3U
#define VADC_ASEN0                 24U
#define VADC_CMS                   8U
#define VADC_ENGT                  0U
#define VADC_TREV                  9U
#define VADC_FLUSH                 10U
#define VADC_ICLSEL                0U
#define VADC_RESREG                16U
#define VADC_RESPOS                21U
#define VADC_VF                    31U

/* ============================== SCU ============================ */
#define SCU_BASE_ADDRESS           (0xF0036000U)
#define REG_SCU_WDTCPU0CON0        (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100U))
#define SCU_ENDINIT                0U
#define SCU_LCK                    1U

/* =========================== APPLICATION ======================= */
#define TURN_NONE                  0U
#define TURN_LEFT                  1U
#define TURN_RIGHT                 2U

/* 12-bit ADC thresholds.  Centre is nominally around 2048. */
#define POT_LEFT_THRESHOLD         1400U
#define POT_CENTER_LOW             1800U
#define POT_CENTER_HIGH            2300U
#define POT_RIGHT_THRESHOLD        2700U
#define POT_REVERSED               0U

IfxCpu_syncEvent cpuSyncEvent = 0;

static void init_io(void);
static void init_vadc_a0(void);
static void vadc_start_a0_conversion(void);
static unsigned int vadc_read_a0(void);
static unsigned int read_sw1(void);
static unsigned int read_sw2(void);
static unsigned int pot_value_for_direction(unsigned int raw);
static unsigned int pot_is_center(unsigned int value);
static void set_turn_leds(unsigned int left_on, unsigned int right_on);
static void set_buzzer(unsigned int on);

void core0_main(void)
{
    unsigned int turn_state = TURN_NONE;
    unsigned int steering_same_side_seen = 0U;
    unsigned int indicator_on = 0U;
    unsigned int last_blink_tick;
    unsigned int prev_sw1;
    unsigned int prev_sw2;

    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!! */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_io();
    init_vadc_a0();

    prev_sw1 = read_sw1();
    prev_sw2 = read_sw2();
    last_blink_tick = REG_STM0_TIM0;

    while (1)
    {
        unsigned int sw1 = read_sw1();
        unsigned int sw2 = read_sw2();
        unsigned int now = REG_STM0_TIM0;
        unsigned int adc_raw;
        unsigned int steering;

        vadc_start_a0_conversion();
        adc_raw = vadc_read_a0();
        steering = pot_value_for_direction(adc_raw);

        /*
         * SW1/SW2 are active-low because the shield switches pull D2/D3 to GND.
         * A falling edge selects the requested direction. Repeated switch bounce
         * only re-selects the same state; it does not toggle it back off.
         */
        if ((prev_sw1 != 0U) && (sw1 == 0U))
        {
            turn_state = TURN_LEFT;
            steering_same_side_seen = 0U;
            indicator_on = 1U;
            last_blink_tick = now;
        }
        else if ((prev_sw2 != 0U) && (sw2 == 0U))
        {
            turn_state = TURN_RIGHT;
            steering_same_side_seen = 0U;
            indicator_on = 1U;
            last_blink_tick = now;
        }

        prev_sw1 = sw1;
        prev_sw2 = sw2;

        if (turn_state != TURN_NONE)
        {
            /* Blink the selected LED at 1 Hz (500 ms ON, 500 ms OFF). */
            if ((unsigned int)(now - last_blink_tick) >= (unsigned int)BLINK_HALF_PERIOD_TICKS)
            {
                last_blink_tick = now;
                indicator_on ^= 1U;
            }
        }
        else
        {
            indicator_on = 0U;
        }

        if (turn_state == TURN_LEFT)
        {
            if (steering <= POT_LEFT_THRESHOLD)
            {
                steering_same_side_seen = 1U;
            }

            /* Opposite steering while LEFT is selected -> warning buzzer. */
            set_buzzer((steering >= POT_RIGHT_THRESHOLD) ? 1U : 0U);

            /* Turned left once, then returned to centre -> self-cancel. */
            if ((steering_same_side_seen != 0U) && (pot_is_center(steering) != 0U))
            {
                turn_state = TURN_NONE;
                indicator_on = 0U;
                set_buzzer(0U);
            }
        }
        else if (turn_state == TURN_RIGHT)
        {
            if (steering >= POT_RIGHT_THRESHOLD)
            {
                steering_same_side_seen = 1U;
            }

            /* Opposite steering while RIGHT is selected -> warning buzzer. */
            set_buzzer((steering <= POT_LEFT_THRESHOLD) ? 1U : 0U);

            /* Turned right once, then returned to centre -> self-cancel. */
            if ((steering_same_side_seen != 0U) && (pot_is_center(steering) != 0U))
            {
                turn_state = TURN_NONE;
                indicator_on = 0U;
                set_buzzer(0U);
            }
        }
        else
        {
            set_buzzer(0U);
        }

        if (turn_state == TURN_LEFT)
        {
            set_turn_leds(indicator_on, 0U);
        }
        else if (turn_state == TURN_RIGHT)
        {
            set_turn_leds(0U, indicator_on);
        }
        else
        {
            set_turn_leds(0U, 0U);
        }
    }
}

static void init_io(void)
{
    /* P02.0 (SW1), P02.1 (SW2): general-purpose inputs with pull-up (PCx=00010B). */
    REG_PORT2_IOCR0 &= ~((0x1FU << PC0_POS) | (0x1FU << PC1_POS));
    REG_PORT2_IOCR0 |=  ((0x02U << PC0_POS) | (0x02U << PC1_POS));

    /* P02.3 (active buzzer): push-pull general-purpose output (PCx=10000B). */
    REG_PORT2_IOCR0 &= ~(0x1FU << PC3_POS);
    REG_PORT2_IOCR0 |=  (0x10U << PC3_POS);

    /* P10.1 (red D12), P10.2 (blue D13): push-pull general-purpose outputs. */
    REG_PORT10_IOCR0 &= ~((0x1FU << PC1_POS) | (0x1FU << PC2_POS));
    REG_PORT10_IOCR0 |=  ((0x10U << PC1_POS) | (0x10U << PC2_POS));

    set_turn_leds(0U, 0U);
    set_buzzer(0U);
}

static unsigned int read_sw1(void)
{
    return ((REG_PORT2_INPUT >> SW1_PIN) & 0x1U);
}

static unsigned int read_sw2(void)
{
    return ((REG_PORT2_INPUT >> SW2_PIN) & 0x1U);
}

static void set_turn_leds(unsigned int left_on, unsigned int right_on)
{
    unsigned int omr = 0U;

    /* TC27x OMR: PSx sets Pn_OUT.Px, PCLx clears it. */
    omr |= left_on  ? (1U << LEFT_LED_PIN)  : (1U << (LEFT_LED_PIN + 16U));
    omr |= right_on ? (1U << RIGHT_LED_PIN) : (1U << (RIGHT_LED_PIN + 16U));
    REG_PORT10_OMR = omr;
}

static void set_buzzer(unsigned int on)
{
    /* Easy Module Shield V1 uses an active buzzer, so a steady HIGH is sufficient. */
    REG_PORT2_OMR = on ? (1U << BUZZER_PIN) : (1U << (BUZZER_PIN + 16U));
}

static unsigned int pot_value_for_direction(unsigned int raw)
{
#if POT_REVERSED
    return (4095U - raw);
#else
    return raw;
#endif
}

static unsigned int pot_is_center(unsigned int value)
{
    return ((value >= POT_CENTER_LOW) && (value <= POT_CENTER_HIGH)) ? 1U : 0U;
}

static void init_vadc_a0(void)
{
    /* Password access -> clear ENDINIT so VADC_CLC can be changed. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << SCU_LCK)) |
                          (1U << SCU_ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << SCU_LCK)) != 0U)
    {
    }

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << SCU_LCK)) &
                          ~(1U << SCU_ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << SCU_LCK)) == 0U)
    {
    }

    REG_VADC_CLC &= ~(1U << VADC_DISR);

    /* Restore ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << SCU_LCK)) |
                          (1U << SCU_ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << SCU_LCK)) != 0U)
    {
    }

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << SCU_LCK)) |
                          (1U << SCU_ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << SCU_LCK)) == 0U)
    {
    }

    while ((REG_VADC_CLC & (1U << VADC_DISS)) != 0U)
    {
    }

    /* Request Source 0: highest priority, wait-for-start mode, arbitration slot enabled. */
    REG_VADC_G4ARBPR |=  (0x3U << VADC_PRIO0);
    REG_VADC_G4ARBPR &= ~(1U << VADC_CSM0);
    REG_VADC_G4ARBPR |=  (1U << VADC_ASEN0);

    /* Queue Source 0 enabled; discard old queue entries. */
    REG_VADC_G4QMR0 &= ~(0x3U << VADC_ENGT);
    REG_VADC_G4QMR0 |=  (0x1U << VADC_ENGT);
    REG_VADC_G4QMR0 |=  (1U << VADC_FLUSH);

    /* Group 4 analog converter normal operation. */
    REG_VADC_G4ARBCFG &= ~(0x3U << VADC_ANONC);
    REG_VADC_G4ARBCFG |=  (0x3U << VADC_ANONC);

    /* Input Class 0: standard 12-bit conversion. */
    REG_VADC_G4ICLASS0 &= ~(0x7U << VADC_CMS);

    /* A0 = SAR4.7: Group 4 Channel 7 -> G4RES1, right-aligned, input class 0. */
    REG_VADC_G4CHCTR7 &= ~(0xFU << VADC_RESREG);
    REG_VADC_G4CHCTR7 |=  (1U << VADC_RESREG);
    REG_VADC_G4CHCTR7 |=  (1U << VADC_RESPOS);
    REG_VADC_G4CHCTR7 &= ~(0x3U << VADC_ICLSEL);
}

static void vadc_start_a0_conversion(void)
{
    /*
     * G4QINR0 is write-only at this shared address (reads return G4QBUR0),
     * therefore use a plain write, not a read-modify-write expression.
     * REQCHNR=7, RF=0, ENSI=0, EXTR=0 -> one conversion of Group 4 Channel 7.
     */
    REG_VADC_G4QINR0 = 0x07U;
    REG_VADC_G4QMR0 |= (1U << VADC_TREV);
}

static unsigned int vadc_read_a0(void)
{
    unsigned int result;

    do
    {
        result = REG_VADC_G4RES1;
    }
    while ((result & (1U << VADC_VF)) == 0U);

    return (result & 0x0FFFU);
}
