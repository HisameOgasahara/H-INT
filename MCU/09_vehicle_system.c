#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * Vehicle system practical exercise (MCU programming slides 328-329)
 *
 * Function A - Turn signals
 *   Potentiometer A0 : steering handle
 *   SW1 D2           : left turn lever
 *   SW2 D3           : right turn lever
 *   LED1 D13 (Blue)  : left turn indicator
 *   LED2 D12 (Red)   : right turn indicator
 *   Active buzzer D5 : turn-signal sound / wrong-direction warning
 *
 * Function B - Auto light
 *   LDR A1           : ambient light sensor (VADC Group 4, channel 6)
 *   RGB LED D9/D10/D11 : head lamp (white = R+G+B)
 *
 * Verified board mapping:
 *   Easy Module A0  -> ShieldBuddy A0  -> SAR4.7 / P32.3
 *   Easy Module A1  -> ShieldBuddy A1  -> SAR4.6 / P32.4
 *   D2  -> P02.0 (SW1)
 *   D3  -> P02.1 (SW2)
 *   D5  -> P02.3 (active buzzer driver)
 *   D9  -> P02.7 (RGB red)
 *   D10 -> P10.5 (RGB green)
 *   D11 -> P10.3 (RGB blue)
 *   D12 -> P10.1 (red LED)
 *   D13 -> P10.2 (blue LED)
 *
 * Easy Module Shield V1 schematic notes:
 *   - SW1/SW2 are active-low.
 *   - LDR A1 is a divider with the photoresistor toward VCC and 10 kOhm
 *     toward GND. Therefore darker conditions produce a lower ADC value.
 *   - D5 drives the onboard active buzzer through a transistor, so a GPIO
 *     high level is sufficient to make sound.
 */

/* ============================ PORTS ============================ */
#define PORT2_BASE_ADDRESS       (0xF003A200u)
#define PORT2_IOCR0              (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10u))
#define PORT2_IOCR4              (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x14u))
#define PORT2_OMR                (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x04u))
#define PORT2_INPUT              (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24u))

#define PORT10_BASE_ADDRESS      (0xF003B000u)
#define PORT10_IOCR0             (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10u))
#define PORT10_IOCR4             (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x14u))
#define PORT10_OMR               (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04u))

/* IOCR control-field bit positions (PCx starts at x*8+3). */
#define PC0_SHIFT                3u
#define PC1_SHIFT                11u
#define PC2_SHIFT                19u
#define PC3_SHIFT                27u
#define PC5_SHIFT                11u
#define PC7_SHIFT                27u

/* Physical pin numbers used by OMR/IN. */
#define P2_SW1                   0u
#define P2_SW2                   1u
#define P2_BUZZER                3u
#define P2_RGB_RED               7u

#define P10_RIGHT_RED            1u
#define P10_LEFT_BLUE            2u
#define P10_RGB_BLUE             3u
#define P10_RGB_GREEN            5u

/* ============================= STM ============================= */
/* STM0 is already clocked in the standard AURIX project setup. */
#define STM0_BASE_ADDRESS        (0xF0000000u)
#define STM0_TIM0                (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10u))
#define STM0_FREQUENCY_HZ        (100000000u)
#define BLINK_HALF_PERIOD_TICKS  (STM0_FREQUENCY_HZ / 2u)  /* 500 ms */

/* ============================= SCU ============================= */
#define SCU_BASE_ADDRESS         (0xF0036000u)
#define SCU_WDTCPU0CON0          (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100u))
#define LCK                      1u
#define ENDINIT                  0u

/* ============================= VADC ============================ */
#define VADC_BASE_ADDRESS        (0xF0020000u)
#define VADC_CLC                 (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x000u))
#define VADC_GLOBCFG             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x080u))
#define VADC_G4ARBCFG            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1480u))
#define VADC_G4ARBPR             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1484u))
#define VADC_G4ICLASS0           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x14A0u))
#define VADC_G4QMR0              (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1504u))
#define VADC_G4QINR0             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1510u))
#define VADC_G4CHCTR6            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1618u))
#define VADC_G4CHCTR7            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x161Cu))
#define VADC_G4RES1              (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1704u))

#define VADC_DISS                1u
#define VADC_DISR                0u
#define ANONC                    0u
#define CAL                      28u
#define CALS                     29u
#define ASEN0                    24u
#define CSM0                     3u
#define PRIO0                    0u
#define CMS                      8u
#define FLUSH                    10u
#define TREV                     9u
#define ENGT                     0u
#define RESPOS                   21u
#define RESREG                   16u
#define ICLSEL                   0u
#define VF                       31u
#define DIVWC                    15u
#define SUCAL                    31u
#define DIVA_VALUE               9u

#define VADC_CH_LDR              6u    /* A1 = SAR4.6 / P32.4 */
#define VADC_CH_STEERING         7u    /* A0 = SAR4.7 / P32.3 */

/* ======================== Application tuning =================== */
#define ADC_MAX_VALUE            4095u

/* Steering zones. A0 is expected near 2048 at the physical center. */
#define STEER_LEFT_MAX           1433u  /* < 35% */
#define STEER_CENTER_LOW         1638u  /* 40% */
#define STEER_CENTER_HIGH        2457u  /* 60% */
#define STEER_RIGHT_MIN          2662u  /* > 65% */

/*
 * LDR: dark -> lower ADC value. Tune this one value for the room if needed.
 * Roughly 1600/4095 of the ADC full scale is used as the initial threshold.
 */
#define LIGHT_THRESHOLD_ADC      1600u

typedef enum
{
    TURN_NONE = 0,
    TURN_LEFT,
    TURN_RIGHT
} TurnState;

typedef enum
{
    STEER_ZONE_LEFT = -1,
    STEER_ZONE_CENTER = 0,
    STEER_ZONE_RIGHT = 1,
    STEER_ZONE_TRANSITION = 2
} SteerZone;

/* Diagnostics: halt CPU0 and inspect these in Variables/Expressions. */
volatile unsigned int g_steering_adc = 0u;
volatile unsigned int g_light_adc = 0u;
volatile unsigned int g_turn_state = TURN_NONE;
volatile unsigned int g_blink_on = 0u;
volatile unsigned int g_headlamp_on = 0u;
volatile unsigned int g_wrong_direction_warning = 0u;

IfxCpu_syncEvent cpuSyncEvent = 0;

static void clear_cpu_endinit(void);
static void set_cpu_endinit(void);
static void init_ports(void);
static void init_vadc(void);
static unsigned int vadc_read_channel(unsigned int channel);
static SteerZone steering_zone(unsigned int adc_value);
static void set_left_indicator(unsigned int on);
static void set_right_indicator(unsigned int on);
static void set_buzzer(unsigned int on);
static void set_headlamp(unsigned int on);

static void clear_cpu_endinit(void)
{
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u)
    {
    }

    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) & ~(1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u)
    {
    }
}

static void set_cpu_endinit(void)
{
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u)
    {
    }

    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u)
    {
    }
}

static void init_ports(void)
{
    /* SW1 P02.0 and SW2 P02.1: general-purpose input with pull-up. */
    PORT2_IOCR0 &= ~((0x1Fu << PC0_SHIFT) | (0x1Fu << PC1_SHIFT));
    PORT2_IOCR0 |=  ((0x02u << PC0_SHIFT) | (0x02u << PC1_SHIFT));

    /* P02.3 buzzer: push-pull general-purpose output. */
    PORT2_IOCR0 &= ~(0x1Fu << PC3_SHIFT);
    PORT2_IOCR0 |=  (0x10u << PC3_SHIFT);

    /* P02.7 RGB red: push-pull general-purpose output. */
    PORT2_IOCR4 &= ~(0x1Fu << PC7_SHIFT);
    PORT2_IOCR4 |=  (0x10u << PC7_SHIFT);

    /* P10.1 right red, P10.2 left blue, P10.3 RGB blue. */
    PORT10_IOCR0 &= ~((0x1Fu << PC1_SHIFT) |
                      (0x1Fu << PC2_SHIFT) |
                      (0x1Fu << PC3_SHIFT));
    PORT10_IOCR0 |=  ((0x10u << PC1_SHIFT) |
                      (0x10u << PC2_SHIFT) |
                      (0x10u << PC3_SHIFT));

    /* P10.5 RGB green. */
    PORT10_IOCR4 &= ~(0x1Fu << PC5_SHIFT);
    PORT10_IOCR4 |=  (0x10u << PC5_SHIFT);

    set_left_indicator(0u);
    set_right_indicator(0u);
    set_buzzer(0u);
    set_headlamp(0u);
}

static void init_vadc(void)
{
    /* VADC_CLC is ENDINIT protected. */
    clear_cpu_endinit();
    VADC_CLC &= ~(1u << VADC_DISR);
    (void)VADC_CLC;
    set_cpu_endinit();

    while ((VADC_CLC & (1u << VADC_DISS)) != 0u)
    {
    }

    /* Group 4 analog converter: 11B = Normal Operation. */
    VADC_G4ARBCFG &= ~(0x3u << ANONC);
    VADC_G4ARBCFG |=  (0x3u << ANONC);

    /* Start-up calibration after reset, following the TC27x VADC sequence. */
    VADC_GLOBCFG = (1u << SUCAL) | (1u << DIVWC) | DIVA_VALUE;
    while (((VADC_G4ARBCFG & (1u << CALS)) == 0u) ||
           ((VADC_G4ARBCFG & (1u << CAL)) != 0u))
    {
    }

    /* Request Source 0: priority 3, wait-for-start, arbitration slot enabled. */
    VADC_G4ARBPR &= ~((0x3u << PRIO0) | (1u << CSM0));
    VADC_G4ARBPR |=  ((0x3u << PRIO0) | (1u << ASEN0));

    /* Queue 0 enabled, stale queue entries flushed. */
    VADC_G4QMR0 &= ~(0x3u << ENGT);
    VADC_G4QMR0 |=  (0x1u << ENGT);
    VADC_G4QMR0 |=  (1u << FLUSH);

    /* Input Class 0: standard 12-bit conversion. */
    VADC_G4ICLASS0 &= ~(0x7u << CMS);

    /* A1 / Group 4 CH6 -> RES1, right aligned, Input Class 0. */
    VADC_G4CHCTR6 &= ~((0xFu << RESREG) | (0x3u << ICLSEL));
    VADC_G4CHCTR6 |=  (1u << RESREG) | (1u << RESPOS);

    /* A0 / Group 4 CH7 -> RES1, right aligned, Input Class 0. */
    VADC_G4CHCTR7 &= ~((0xFu << RESREG) | (0x3u << ICLSEL));
    VADC_G4CHCTR7 |=  (1u << RESREG) | (1u << RESPOS);
}

static unsigned int vadc_read_channel(unsigned int channel)
{
    unsigned int result_register;

    /*
     * G4QINR0 shares its address with QBUR0 on reads. It must therefore be
     * written directly instead of using a read-modify-write expression.
     * RF=0 gives one conversion request.
     */
    VADC_G4QINR0 = (channel & 0x1Fu);
    VADC_G4QMR0 |= (1u << TREV);

    do
    {
        result_register = VADC_G4RES1;
    }
    while ((result_register & (1u << VF)) == 0u);

    return (result_register & 0x0FFFu);
}

static SteerZone steering_zone(unsigned int adc_value)
{
    if (adc_value <= STEER_LEFT_MAX)
    {
        return STEER_ZONE_LEFT;
    }
    if (adc_value >= STEER_RIGHT_MIN)
    {
        return STEER_ZONE_RIGHT;
    }
    if ((adc_value >= STEER_CENTER_LOW) && (adc_value <= STEER_CENTER_HIGH))
    {
        return STEER_ZONE_CENTER;
    }

    return STEER_ZONE_TRANSITION;
}

static void set_left_indicator(unsigned int on)
{
    PORT10_OMR = on ? (1u << P10_LEFT_BLUE) : (1u << (P10_LEFT_BLUE + 16u));
}

static void set_right_indicator(unsigned int on)
{
    PORT10_OMR = on ? (1u << P10_RIGHT_RED) : (1u << (P10_RIGHT_RED + 16u));
}

static void set_buzzer(unsigned int on)
{
    PORT2_OMR = on ? (1u << P2_BUZZER) : (1u << (P2_BUZZER + 16u));
}

static void set_headlamp(unsigned int on)
{
    unsigned int port10_omr = 0u;

    /* White head lamp = RGB red + green + blue together. */
    PORT2_OMR = on ? (1u << P2_RGB_RED) : (1u << (P2_RGB_RED + 16u));

    port10_omr |= on ? (1u << P10_RGB_GREEN) : (1u << (P10_RGB_GREEN + 16u));
    port10_omr |= on ? (1u << P10_RGB_BLUE)  : (1u << (P10_RGB_BLUE + 16u));
    PORT10_OMR = port10_omr;
}

int core0_main(void)
{
    TurnState turn_state = TURN_NONE;
    SteerZone steer;
    unsigned int left_seen = 0u;
    unsigned int right_seen = 0u;
    unsigned int last_sw1 = 1u;
    unsigned int last_sw2 = 1u;
    unsigned int blink_on = 0u;
    unsigned int last_blink_tick;
    unsigned int now;
    unsigned int sw1;
    unsigned int sw2;
    unsigned int wrong_direction;

    IfxCpu_enableInterrupts();

    /* Practical-exercise setup: watchdogs are disabled, as in preceding labs. */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_ports();
    init_vadc();

    last_blink_tick = STM0_TIM0;

    while (1)
    {
        /* Read both analog functions independently from Group 4. */
        g_steering_adc = vadc_read_channel(VADC_CH_STEERING);
        g_light_adc = vadc_read_channel(VADC_CH_LDR);
        steer = steering_zone(g_steering_adc);

        /* ---------------- Function B: auto light ---------------- */
        g_headlamp_on = (g_light_adc < LIGHT_THRESHOLD_ADC) ? 1u : 0u;
        set_headlamp(g_headlamp_on);

        /* ---------------- Function A: lever input ---------------- */
        sw1 = (PORT2_INPUT >> P2_SW1) & 0x1u;
        sw2 = (PORT2_INPUT >> P2_SW2) & 0x1u;

        /* Falling edge: released(1) -> pressed(0). */
        if ((last_sw1 != 0u) && (sw1 == 0u))
        {
            turn_state = TURN_LEFT;
            left_seen = 0u;
            right_seen = 0u;
            blink_on = 1u;
            last_blink_tick = STM0_TIM0;
        }
        else if ((last_sw2 != 0u) && (sw2 == 0u))
        {
            turn_state = TURN_RIGHT;
            left_seen = 0u;
            right_seen = 0u;
            blink_on = 1u;
            last_blink_tick = STM0_TIM0;
        }

        last_sw1 = sw1;
        last_sw2 = sw2;

        /* Remember that the steering wheel actually entered the selected side. */
        if ((turn_state == TURN_LEFT) && (steer == STEER_ZONE_LEFT))
        {
            left_seen = 1u;
        }
        else if ((turn_state == TURN_RIGHT) && (steer == STEER_ZONE_RIGHT))
        {
            right_seen = 1u;
        }

        /* Return-to-center cancellation after the selected steering motion. */
        if ((turn_state == TURN_LEFT) && (left_seen != 0u) &&
            (steer == STEER_ZONE_CENTER))
        {
            turn_state = TURN_NONE;
            blink_on = 0u;
        }
        else if ((turn_state == TURN_RIGHT) && (right_seen != 0u) &&
                 (steer == STEER_ZONE_CENTER))
        {
            turn_state = TURN_NONE;
            blink_on = 0u;
        }

        /* Wrong steering direction while a turn lever is active -> warning. */
        wrong_direction = 0u;
        if ((turn_state == TURN_LEFT) && (steer == STEER_ZONE_RIGHT))
        {
            wrong_direction = 1u;
        }
        else if ((turn_state == TURN_RIGHT) && (steer == STEER_ZONE_LEFT))
        {
            wrong_direction = 1u;
        }
        g_wrong_direction_warning = wrong_direction;

        /* Non-blocking 1 Hz turn-indicator blink using STM0 TIM0. */
        now = STM0_TIM0;
        if ((turn_state != TURN_NONE) &&
            ((unsigned int)(now - last_blink_tick) >= BLINK_HALF_PERIOD_TICKS))
        {
            last_blink_tick = now;
            blink_on ^= 1u;
        }

        if (turn_state == TURN_LEFT)
        {
            set_left_indicator(blink_on);
            set_right_indicator(0u);
        }
        else if (turn_state == TURN_RIGHT)
        {
            set_left_indicator(0u);
            set_right_indicator(blink_on);
        }
        else
        {
            set_left_indicator(0u);
            set_right_indicator(0u);
        }

        /*
         * Normal: buzzer follows the indicator blink as the turn-signal sound.
         * Wrong steering direction: buzzer is forced continuously ON as warning.
         */
        if (wrong_direction != 0u)
        {
            set_buzzer(1u);
        }
        else if ((turn_state != TURN_NONE) && (blink_on != 0u))
        {
            set_buzzer(1u);
        }
        else
        {
            set_buzzer(0u);
        }

        g_turn_state = (unsigned int)turn_state;
        g_blink_on = blink_on;
    }

    return 1;
}
