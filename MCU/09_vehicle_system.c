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
 *   Buzzer D5        : warning only when steering opposite to selected direction
 *
 * Function B - Auto light
 *   LDR A1             : ambient light sensor, VADC Group 4 Channel 6
 *   RGB LED D9/D10/D11 : head lamp (white = R+G+B)
 *
 * Board mapping verified from ShieldBuddy TC275 + Easy Module Shield V1:
 *   A0  -> SAR4.7 / P32.3
 *   A1  -> SAR4.6 / P32.4
 *   D2  -> P02.0
 *   D3  -> P02.1
 *   D5  -> P02.3 -> TOUT3 -> GTM TOM0_CH11
 *   D9  -> P02.7
 *   D10 -> P10.5
 *   D11 -> P10.3
 *   D12 -> P10.1
 *   D13 -> P10.2
 *
 * Important hardware details:
 *   - SW1/SW2 are active-low.
 *   - The shield schematic shows the LDR from VCC to A1 and 10 kOhm from
 *     A1 to GND: bright -> higher ADC, dark -> lower ADC.
 *   - D5 is driven through the same verified GTM TOM0_CH11 -> TOUT3 -> P02.3
 *     PWM path used by the preceding buzzer exercise.
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

#define PC0_SHIFT                3u
#define PC1_SHIFT                11u
#define PC2_SHIFT                19u
#define PC3_SHIFT                27u
#define PC5_SHIFT                11u
#define PC7_SHIFT                27u

#define P2_SW1                   0u
#define P2_SW2                   1u
#define P2_RGB_RED               7u

#define P10_RIGHT_RED            1u
#define P10_LEFT_BLUE            2u
#define P10_RGB_BLUE             3u
#define P10_RGB_GREEN            5u

/* ============================= STM ============================= */
#define STM0_BASE_ADDRESS        (0xF0000000u)
#define STM0_TIM0                (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10u))
#define STM0_FREQUENCY_HZ        (100000000u)
#define BLINK_HALF_PERIOD_TICKS  (STM0_FREQUENCY_HZ / 2u)

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
#define VADC_G4RES0              (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1700u))
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
#define CHNR                     20u
#define DIVWC                    15u
#define SUCAL                    31u
#define DIVA_VALUE               9u

#define VADC_CH_LDR              6u
#define VADC_CH_STEERING         7u

/* ======================== GTM buzzer PWM ======================= */
#define GTM_BASE_ADDRESS              (0xF0100000u)
#define GTM_CLC                       (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00u))
#define GTM_TOUTSEL0                  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD30u))
#define GTM_CMU_CLK_EN                (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300u))
#define GTM_CMU_FXCLK_CTRL            (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344u))

#define GTM_TOM0_TGC1_GLB_CTRL        (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08230u))
#define GTM_TOM0_TGC1_FUPD_CTRL       (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08238u))
#define GTM_TOM0_TGC1_ENDIS_CTRL      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08270u))
#define GTM_TOM0_TGC1_OUTEN_CTRL      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08278u))
#define GTM_TOM0_CH11_CTRL            (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C0u))
#define GTM_TOM0_CH11_SR0             (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C4u))
#define GTM_TOM0_CH11_SR1             (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C8u))
#define GTM_TOM0_CH11_CM1             (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082D0u))

#define GTM_DISS                       1u
#define GTM_DISR                       0u
#define EN_FXCLK                       22u
#define FXCLK_SEL                      0u
#define SEL3                           6u
#define UPEN_CTRL3                     22u
#define HOST_TRIG                      0u
#define ENDIS_CTRL3                    6u
#define OUTEN_CTRL3                    6u
#define FUPD_CTRL3                     6u
#define RSTCN0_CH3                     22u
#define CLK_SRC_SR                     12u
#define SL                             11u

/* Same verified 1 kHz / 50% PWM path as 07_pwm_control_3_buzzer_button.c. */
#define BUZZER_PWM_PERIOD              6250u
#define BUZZER_PWM_DUTY                3125u

/* ======================== Application tuning =================== */
#define STEER_LEFT_MAX           1433u
#define STEER_CENTER_LOW         1638u
#define STEER_CENTER_HIGH        2457u
#define STEER_RIGHT_MIN          2662u

/* LDR divider: dark -> lower ADC. Hysteresis avoids chatter near threshold. */
#define LIGHT_ON_THRESHOLD_ADC   2048u
#define LIGHT_OFF_THRESHOLD_ADC  2300u

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
static void init_buzzer_pwm(void);
static unsigned int vadc_read_channel(unsigned int channel);
static SteerZone steering_zone(unsigned int adc_value);
static void set_left_indicator(unsigned int on);
static void set_right_indicator(unsigned int on);
static void set_buzzer(unsigned int on);
static void set_headlamp(unsigned int on);

static void clear_cpu_endinit(void)
{
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u) { }

    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) & ~(1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u) { }
}

static void set_cpu_endinit(void)
{
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u) { }

    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u) { }
}

static void init_ports(void)
{
    /* SW1/SW2: input with pull-up. */
    PORT2_IOCR0 &= ~((0x1Fu << PC0_SHIFT) | (0x1Fu << PC1_SHIFT));
    PORT2_IOCR0 |=  ((0x02u << PC0_SHIFT) | (0x02u << PC1_SHIFT));

    /* D5 / P02.3: alternate output function 1 = GTM TOUT3. */
    PORT2_IOCR0 &= ~(0x1Fu << PC3_SHIFT);
    PORT2_IOCR0 |=  (0x11u << PC3_SHIFT);

    /* RGB red D9 / P02.7: GPIO output. */
    PORT2_IOCR4 &= ~(0x1Fu << PC7_SHIFT);
    PORT2_IOCR4 |=  (0x10u << PC7_SHIFT);

    /* D12/D13 indicators and RGB blue. */
    PORT10_IOCR0 &= ~((0x1Fu << PC1_SHIFT) |
                      (0x1Fu << PC2_SHIFT) |
                      (0x1Fu << PC3_SHIFT));
    PORT10_IOCR0 |=  ((0x10u << PC1_SHIFT) |
                      (0x10u << PC2_SHIFT) |
                      (0x10u << PC3_SHIFT));

    /* RGB green D10 / P10.5. */
    PORT10_IOCR4 &= ~(0x1Fu << PC5_SHIFT);
    PORT10_IOCR4 |=  (0x10u << PC5_SHIFT);

    set_left_indicator(0u);
    set_right_indicator(0u);
    set_headlamp(0u);
}

static void init_vadc(void)
{
    clear_cpu_endinit();
    VADC_CLC &= ~(1u << VADC_DISR);
    (void)VADC_CLC;
    set_cpu_endinit();

    while ((VADC_CLC & (1u << VADC_DISS)) != 0u) { }

    VADC_G4ARBCFG &= ~(0x3u << ANONC);
    VADC_G4ARBCFG |=  (0x3u << ANONC);

    VADC_GLOBCFG = (1u << SUCAL) | (1u << DIVWC) | DIVA_VALUE;
    while (((VADC_G4ARBCFG & (1u << CALS)) == 0u) ||
           ((VADC_G4ARBCFG & (1u << CAL)) != 0u))
    {
    }

    VADC_G4ARBPR &= ~((0x3u << PRIO0) | (1u << CSM0));
    VADC_G4ARBPR |=  ((0x3u << PRIO0) | (1u << ASEN0));

    VADC_G4QMR0 &= ~(0x3u << ENGT);
    VADC_G4QMR0 |=  (0x1u << ENGT);
    VADC_G4QMR0 |=  (1u << FLUSH);

    VADC_G4ICLASS0 &= ~(0x7u << CMS);

    /*
     * Give the two inputs different result registers.
     * CH6 (LDR) -> G4RES0
     * CH7 (steering) -> G4RES1
     */
    VADC_G4CHCTR6 &= ~((0xFu << RESREG) | (0x3u << ICLSEL));
    VADC_G4CHCTR6 |=  (0u << RESREG) | (1u << RESPOS);

    VADC_G4CHCTR7 &= ~((0xFu << RESREG) | (0x3u << ICLSEL));
    VADC_G4CHCTR7 |=  (1u << RESREG) | (1u << RESPOS);
}

static void init_buzzer_pwm(void)
{
    clear_cpu_endinit();
    GTM_CLC &= ~(1u << GTM_DISR);
    set_cpu_endinit();

    while ((GTM_CLC & (1u << GTM_DISS)) != 0u) { }

    GTM_CMU_FXCLK_CTRL &= ~(0xFu << FXCLK_SEL);
    GTM_CMU_CLK_EN |= (0x2u << EN_FXCLK);

    /* TOM0_CH11 is local channel 3 of TGC1. */
    GTM_TOM0_TGC1_GLB_CTRL |= (0x2u << UPEN_CTRL3);
    GTM_TOM0_TGC1_FUPD_CTRL |= (0x2u << FUPD_CTRL3);
    GTM_TOM0_TGC1_FUPD_CTRL |= (0x2u << RSTCN0_CH3);
    GTM_TOM0_TGC1_ENDIS_CTRL |= (0x2u << ENDIS_CTRL3);
    GTM_TOM0_TGC1_OUTEN_CTRL |= (0x2u << OUTEN_CTRL3);

    GTM_TOM0_CH11_CTRL |= (1u << SL);
    GTM_TOM0_CH11_CTRL &= ~(0x7u << CLK_SRC_SR);
    GTM_TOM0_CH11_CTRL |=  (1u << CLK_SRC_SR);  /* CMU_FXCLK1 = 6.25 MHz */

    GTM_TOM0_CH11_SR0 = BUZZER_PWM_PERIOD;
    GTM_TOM0_CH11_SR1 = 0u;
    GTM_TOM0_CH11_CM1 = 0u;

    /* TOUT3 <- TOM0_CH11. */
    GTM_TOUTSEL0 &= ~(0x3u << SEL3);

    GTM_TOM0_TGC1_GLB_CTRL |= (1u << HOST_TRIG);
}

static unsigned int vadc_read_channel(unsigned int channel)
{
    unsigned int result_register;

    VADC_G4QINR0 = (channel & 0x1Fu);
    VADC_G4QMR0 |= (1u << TREV);

    if (channel == VADC_CH_LDR)
    {
        do
        {
            result_register = VADC_G4RES0;
        }
        while (((result_register & (1u << VF)) == 0u) ||
               (((result_register >> CHNR) & 0x1Fu) != VADC_CH_LDR));
    }
    else
    {
        do
        {
            result_register = VADC_G4RES1;
        }
        while (((result_register & (1u << VF)) == 0u) ||
               (((result_register >> CHNR) & 0x1Fu) != VADC_CH_STEERING));
    }

    return (result_register & 0x0FFFu);
}

static SteerZone steering_zone(unsigned int adc_value)
{
    if (adc_value <= STEER_LEFT_MAX) return STEER_ZONE_LEFT;
    if (adc_value >= STEER_RIGHT_MIN) return STEER_ZONE_RIGHT;
    if ((adc_value >= STEER_CENTER_LOW) && (adc_value <= STEER_CENTER_HIGH))
        return STEER_ZONE_CENTER;
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
    unsigned int duty = (on != 0u) ? BUZZER_PWM_DUTY : 0u;

    /* Keep shadow and compare register equal so ON/OFF takes effect immediately. */
    GTM_TOM0_CH11_SR1 = duty;
    GTM_TOM0_CH11_CM1 = duty;
}

static void set_headlamp(unsigned int on)
{
    unsigned int port10_omr = 0u;

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
    unsigned int headlamp_on = 0u;
    unsigned int last_blink_tick;
    unsigned int now;
    unsigned int sw1;
    unsigned int sw2;
    unsigned int wrong_direction;

    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_ports();
    init_vadc();
    init_buzzer_pwm();
    set_buzzer(0u);

    last_blink_tick = STM0_TIM0;

    while (1)
    {
        /* -------- Read both analog inputs independently -------- */
        g_steering_adc = vadc_read_channel(VADC_CH_STEERING);
        g_light_adc = vadc_read_channel(VADC_CH_LDR);
        steer = steering_zone(g_steering_adc);

        /* ---------------- Function B: auto light ---------------- */
        if ((headlamp_on == 0u) && (g_light_adc <= LIGHT_ON_THRESHOLD_ADC))
        {
            headlamp_on = 1u;
        }
        else if ((headlamp_on != 0u) && (g_light_adc >= LIGHT_OFF_THRESHOLD_ADC))
        {
            headlamp_on = 0u;
        }
        g_headlamp_on = headlamp_on;
        set_headlamp(headlamp_on);

        /* ---------------- Function A: lever input ---------------- */
        sw1 = (PORT2_INPUT >> P2_SW1) & 0x1u;
        sw2 = (PORT2_INPUT >> P2_SW2) & 0x1u;

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

        if ((turn_state == TURN_LEFT) && (steer == STEER_ZONE_LEFT))
            left_seen = 1u;
        else if ((turn_state == TURN_RIGHT) && (steer == STEER_ZONE_RIGHT))
            right_seen = 1u;

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

        /* Slide requirement: warning only when steering opposite to selected direction. */
        wrong_direction = 0u;
        if ((turn_state == TURN_LEFT) && (steer == STEER_ZONE_RIGHT))
            wrong_direction = 1u;
        else if ((turn_state == TURN_RIGHT) && (steer == STEER_ZONE_LEFT))
            wrong_direction = 1u;
        g_wrong_direction_warning = wrong_direction;

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
         * Follow the slide exactly:
         * SW1/SW2 alone only selects/blinks the indicator.
         * The buzzer sounds only while the steering handle is turned to
         * the direction opposite to the selected lever.
         */
        set_buzzer(wrong_direction);

        g_turn_state = (unsigned int)turn_state;
        g_blink_on = blink_on;
    }

    return 1;
}
