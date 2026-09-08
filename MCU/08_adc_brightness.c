#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * ADC practical exercise (slide 327)
 * Potentiometer(A0) -> VADC G4 CH7 -> PWM duty -> LED2(D12)
 *
 * Source-verified mapping:
 *   Easy Module Shield A0  -> Potentiometer
 *   ShieldBuddy A0         -> SAR4.7 / P32.3
 *   Easy Module Shield D12 -> LED2 (Red)
 *   ShieldBuddy D12        -> P10.1
 *   P10.1                  -> TOUT103 -> TOM0_CH1 (Timer A)
 *
 * This intentionally reuses the same P10.1/TOM0_CH1 PWM path taught
 * in the preceding PWM project and changes only the duty from ADC data.
 */

/* ---------------- PORT10 ---------------- */
#define PORT10_BASE_ADDRESS      (0xF003B000u)
#define PORT10_IOCR0             (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10u))
#define PC1                      11u

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

#define GTM_TOM0_TGC0_GLB_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08030u))
#define GTM_TOM0_TGC0_ENDIS_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08070u))
#define GTM_TOM0_TGC0_OUTEN_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08078u))
#define GTM_TOM0_TGC0_FUPD_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08038u))

#define GTM_TOM0_CH1_CTRL         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08040u))
#define GTM_TOM0_CH1_SR0          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08044u))
#define GTM_TOM0_CH1_SR1          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08048u))

#define HOST_TRIG                 0u
#define UPEN_CTRL1                18u
#define ENDIS_CTRL1               2u
#define OUTEN_CTRL1               2u
#define FUPD_CTRL1                2u
#define RSTCN0_CH1                18u
#define CLK_SRC_SR                12u
#define SL                        11u
#define SEL7                      14u

#define PWM_PERIOD_TICKS          12500u
#define ADC_MAX_VALUE             4095u

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_VADC(void);
void init_PWM_LED(void);
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
    init_PWM_LED();

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

    VADC_G4ARBPR |=  (0x3u << PRIO0);
    VADC_G4ARBPR &= ~(1u << CSM0);
    VADC_G4ARBPR |=  (1u << ASEN0);

    VADC_G4QMR0 &= ~(0x3u << ENGT);
    VADC_G4QMR0 |=  (0x1u << ENGT);
    VADC_G4QMR0 |=  (1u << FLUSH);

    VADC_G4ARBCFG &= ~(0x3u << ANONC);
    VADC_G4ARBCFG |=  (0x3u << ANONC);

    VADC_G4ICLASS0 &= ~(0x7u << CMS);

    VADC_G4CHCTR7 &= ~(0xFu << RESREG);
    VADC_G4CHCTR7 |=  (1u << RESREG);
    VADC_G4CHCTR7 |=  (1u << RESPOS);
    VADC_G4CHCTR7 &= ~(0x3u << ICLSEL);
}

void init_PWM_LED(void)
{
    /* P10.1 -> TOUT103, alternate output function 1 */
    PORT10_IOCR0 &= ~(0x1Fu << PC1);
    PORT10_IOCR0 |=  (0x11u << PC1);

    clear_cpu_endinit();
    GTM_CLC &= ~(1u << GTM_DISR);
    set_cpu_endinit();

    while ((GTM_CLC & (1u << GTM_DISS)) != 0u);

    GTM_CMU_FXCLK_CTRL &= ~(0xFu << FXCLK_SEL);
    GTM_CMU_CLK_EN |= (0x2u << EN_FXCLK);

    GTM_TOM0_TGC0_GLB_CTRL &= ~(0x3u << UPEN_CTRL1);
    GTM_TOM0_TGC0_GLB_CTRL |=  (0x2u << UPEN_CTRL1);

    GTM_TOM0_TGC0_FUPD_CTRL &= ~((0x3u << FUPD_CTRL1) | (0x3u << RSTCN0_CH1));
    GTM_TOM0_TGC0_FUPD_CTRL |=  ((0x2u << FUPD_CTRL1) | (0x2u << RSTCN0_CH1));

    GTM_TOM0_TGC0_ENDIS_CTRL &= ~(0x3u << ENDIS_CTRL1);
    GTM_TOM0_TGC0_ENDIS_CTRL |=  (0x2u << ENDIS_CTRL1);
    GTM_TOM0_TGC0_OUTEN_CTRL &= ~(0x3u << OUTEN_CTRL1);
    GTM_TOM0_TGC0_OUTEN_CTRL |=  (0x2u << OUTEN_CTRL1);

    GTM_TOM0_CH1_CTRL |=  (1u << SL);
    GTM_TOM0_CH1_CTRL &= ~(0x7u << CLK_SRC_SR);
    GTM_TOM0_CH1_CTRL |=  (0x1u << CLK_SRC_SR);

    GTM_TOM0_CH1_SR0 = PWM_PERIOD_TICKS;
    GTM_TOM0_CH1_SR1 = 0u;

    /* TOUT103 = TOUTSEL6.SEL7; 00B selects Timer A = TOM0_CH1 */
    GTM_TOUTSEL6 &= ~(0x3u << SEL7);

    GTM_TOM0_TGC0_GLB_CTRL |= (1u << HOST_TRIG);
}

void VADC_start_conversion(void)
{
    VADC_G4QINR0 = 0x07u;
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

    duty_ticks = (adc_value * PWM_PERIOD_TICKS) / ADC_MAX_VALUE;

    /* With UPEN enabled, SR1 is copied to CM1 at the next CN0 reset. */
    GTM_TOM0_CH1_SR1 = duty_ticks;
}
