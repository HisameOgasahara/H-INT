#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * ADC practical exercise (slide 327)
 * Potentiometer(A0) -> VADC G4 CH7 -> PWM duty -> RGB Blue LED(D11)
 *
 * ShieldBuddy / Easy Module Shield mapping:
 *   A0  -> SAR4.7 / P32.3
 *   D11 -> P10.3 (RGB Blue LED)
 *
 * TC27x GTM mapping:
 *   P10.3 -> TOUT105 -> TOM0_CH3 (Timer A selection)
 */

/* ---------------- PORT10 ---------------- */
#define PORT10_BASE_ADDRESS      (0xF003B000u)
#define PORT10_IOCR0             (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10u))
#define PC3                      27u

/* ---------------- SCU / WDT ---------------- */
#define SCU_BASE_ADDRESS         (0xF0036000u)
#define SCU_WDTCPU0CON0          (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100u))
#define LCK                      1u
#define ENDINIT                  0u

/* ---------------- VADC ---------------- */
#define VADC_BASE_ADDRESS        (0xF0020000u)
#define VADC_CLC                 (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x000u))
#define VADC_G4ARBCFG            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1480u))
#define VADC_G4ARBPR             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1484u))
#define VADC_G4ICLASS0           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x14A0u))
#define VADC_G4QMR0              (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1504u))
#define VADC_G4QINR0             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1510u))
#define VADC_G4CHCTR7            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x161Cu))
#define VADC_G4RES1              (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1704u))

#define VADC_DISS                1u
#define VADC_DISR                0u
#define ANONC                    0u
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

/* ---------------- GTM / CMU ---------------- */
#define GTM_BASE_ADDRESS         (0xF0100000u)
#define GTM_CLC                  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00u))
#define GTM_TOUTSEL6             (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD48u))
#define GTM_CMU_CLK_EN           (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300u))
#define GTM_CMU_FXCLK_CTRL       (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344u))

#define GTM_DISS                 1u
#define GTM_DISR                 0u
#define EN_FXCLK                 22u
#define FXCLK_SEL                0u

/* TOM0 TGC0 controls channels 0..7 */
#define GTM_TOM0_TGC0_GLB_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08030u))
#define GTM_TOM0_TGC0_ENDIS_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08070u))
#define GTM_TOM0_TGC0_OUTEN_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08078u))
#define GTM_TOM0_TGC0_FUPD_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08038u))

/* TOM0 channel 3: CTRL=08000+3*40, SR0=+4, SR1=+8 */
#define GTM_TOM0_CH3_CTRL         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x080C0u))
#define GTM_TOM0_CH3_SR0          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x080C4u))
#define GTM_TOM0_CH3_SR1          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x080C8u))

#define HOST_TRIG                 0u
#define UPEN_CTRL3                22u
#define ENDIS_CTRL3               6u
#define OUTEN_CTRL3               6u
#define FUPD_CTRL3                6u
#define RSTCN0_CH3                22u
#define CLK_SRC_SR                12u
#define SL                        11u
#define SEL9                      18u

/* PWM period: FXCLK1 = 6.25 MHz, 12500 ticks -> 500 Hz */
#define PWM_PERIOD_TICKS          12500u
#define ADC_MAX_VALUE             4095u

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_VADC(void);
void init_PWM_BlueLED(void);
void VADC_start_conversion(void);
unsigned int VADC_read_result(void);
void set_LED_brightness_from_ADC(unsigned int adc_value);

static void clear_cpu_endinit(void)
{
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u);

    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) & ~(1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u);
}

static void set_cpu_endinit(void)
{
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u);

    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u);
}

void core0_main(void)
{
    unsigned int adc_result;

    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_VADC();
    init_PWM_BlueLED();

    while (1)
    {
        VADC_start_conversion();
        adc_result = VADC_read_result();
        set_LED_brightness_from_ADC(adc_result);
    }
}

void init_VADC(void)
{
    clear_cpu_endinit();
    VADC_CLC &= ~(1u << VADC_DISR);
    set_cpu_endinit();

    while ((VADC_CLC & (1u << VADC_DISS)) != 0u);

    /* Request Source 0: highest priority, wait-for-start, arbitration enabled */
    VADC_G4ARBPR |=  (0x3u << PRIO0);
    VADC_G4ARBPR &= ~(1u << CSM0);
    VADC_G4ARBPR |=  (1u << ASEN0);

    /* Queue requests enabled, remove old queue contents */
    VADC_G4QMR0 &= ~(0x3u << ENGT);
    VADC_G4QMR0 |=  (0x1u << ENGT);
    VADC_G4QMR0 |=  (1u << FLUSH);

    /* Group 4 converter normal operation */
    VADC_G4ARBCFG &= ~(0x3u << ANONC);
    VADC_G4ARBCFG |=  (0x3u << ANONC);

    /* Input Class 0: 12-bit standard conversion */
    VADC_G4ICLASS0 &= ~(0x7u << CMS);

    /* G4 CH7 -> RES1, right aligned, Input Class 0 */
    VADC_G4CHCTR7 &= ~(0xFu << RESREG);
    VADC_G4CHCTR7 |=  (1u << RESREG);
    VADC_G4CHCTR7 |=  (1u << RESPOS);
    VADC_G4CHCTR7 &= ~(0x3u << ICLSEL);
}

void init_PWM_BlueLED(void)
{
    /* P10.3: GTM TOUT105, alternate output function 1, push-pull */
    PORT10_IOCR0 &= ~(0x1Fu << PC3);
    PORT10_IOCR0 |=  (0x11u << PC3);

    clear_cpu_endinit();
    GTM_CLC &= ~(1u << GTM_DISR);
    set_cpu_endinit();

    while ((GTM_CLC & (1u << GTM_DISS)) != 0u);

    /* Enable CMU fixed clocks; select global clock as FXCLK input */
    GTM_CMU_FXCLK_CTRL &= ~(0xFu << FXCLK_SEL);
    GTM_CMU_CLK_EN |= (0x2u << EN_FXCLK);

    /* TOM0 CH3: shadow-register update enabled */
    GTM_TOM0_TGC0_GLB_CTRL &= ~(0x3u << UPEN_CTRL3);
    GTM_TOM0_TGC0_GLB_CTRL |=  (0x2u << UPEN_CTRL3);

    /* Force-update and reset CN0 on the first update */
    GTM_TOM0_TGC0_FUPD_CTRL &= ~((0x3u << FUPD_CTRL3) | (0x3u << RSTCN0_CH3));
    GTM_TOM0_TGC0_FUPD_CTRL |=  ((0x2u << FUPD_CTRL3) | (0x2u << RSTCN0_CH3));

    /* Enable channel 3 and its output on HOST_TRIG */
    GTM_TOM0_TGC0_ENDIS_CTRL &= ~(0x3u << ENDIS_CTRL3);
    GTM_TOM0_TGC0_ENDIS_CTRL |=  (0x2u << ENDIS_CTRL3);
    GTM_TOM0_TGC0_OUTEN_CTRL &= ~(0x3u << OUTEN_CTRL3);
    GTM_TOM0_TGC0_OUTEN_CTRL |=  (0x2u << OUTEN_CTRL3);

    /* Active-high PWM, clock source = FXCLK1 (6.25 MHz) */
    GTM_TOM0_CH3_CTRL |=  (1u << SL);
    GTM_TOM0_CH3_CTRL &= ~(0x7u << CLK_SRC_SR);
    GTM_TOM0_CH3_CTRL |=  (0x1u << CLK_SRC_SR);

    /* 500 Hz PWM, initially 0% duty */
    GTM_TOM0_CH3_SR0 = PWM_PERIOD_TICKS;
    GTM_TOM0_CH3_SR1 = 0u;

    /* TOUT105 = TOUTSEL6.SEL9, Timer A = TOM0_CH3 */
    GTM_TOUTSEL6 &= ~(0x3u << SEL9);

    /* Apply initial shadow values and channel/output enable requests */
    GTM_TOM0_TGC0_GLB_CTRL |= (1u << HOST_TRIG);
}

void VADC_start_conversion(void)
{
    /* Shared QINR0/QBUR0 address: write the queue entry directly, no RMW. */
    VADC_G4QINR0 = 0x07u;  /* REQCHNR=7, one-shot */
    VADC_G4QMR0 |= (1u << TREV);
}

unsigned int VADC_read_result(void)
{
    unsigned int result_register;

    do
    {
        result_register = VADC_G4RES1;
    }
    while ((result_register & (1u << VF)) == 0u);

    return (result_register & 0x0FFFu);
}

void set_LED_brightness_from_ADC(unsigned int adc_value)
{
    unsigned int duty_ticks;

    if (adc_value > ADC_MAX_VALUE)
    {
        adc_value = ADC_MAX_VALUE;
    }

    /* Map ADC 0..4095 linearly to PWM duty 0..12500 ticks. */
    duty_ticks = (adc_value * PWM_PERIOD_TICKS) / ADC_MAX_VALUE;

    /*
     * TC27x TOM: CM1=0 -> 0% duty, CM1>=CM0 -> 100% duty.
     * With UPEN enabled, SR1 is transferred safely at the next CN0 reset.
     */
    GTM_TOM0_CH3_SR1 = duty_ticks;
}
