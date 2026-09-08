#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * PWM Control Exercise I
 * D12 red LED -> ShieldBuddy -> TC275 P10.1 -> TOUT103 -> GTM TOM0 CH1
 * STM0 compare interrupt changes PWM duty every 10 ms.
 *
 * NOTE:
 * The AURIX project headers already define many SFR names such as
 * SCU_WDTCPU0CON0 and STM0_TIM0.  Therefore all direct-register macros in
 * this file use a REG_ prefix to avoid macro redefinition conflicts.
 */

/* System Control Unit */
#define SCU_BASE_ADDRESS          (0xF0036000U)
#define REG_SCU_WDTCPU0CON0       (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100U))

#define LCK                       1U
#define ENDINIT                   0U

/* Generic Timer Module (GTM) Control Registers */
#define GTM_BASE_ADDRESS          (0xF0100000U)
#define REG_GTM_CLC               (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00U))
#define REG_GTM_TOUTSEL6          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD48U))

#define DISS                      1U
#define DISR                      0U
#define SEL7                      14U

/* GTM - Clock Management Unit (CMU) */
#define REG_GTM_CMU_CLK_EN        (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300U))
#define REG_GTM_CMU_FXCLK_CTRL    (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344U))

#define EN_FXCLK                  22U
#define FXCLK_SEL                 0U

/* GTM - Timer Output Module (TOM0 CH1) */
#define REG_GTM_TOM0_TGC0_GLB_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08030U))
#define REG_GTM_TOM0_TGC0_ENDIS_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08070U))
#define REG_GTM_TOM0_TGC0_OUTEN_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08078U))
#define REG_GTM_TOM0_TGC0_FUPD_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08038U))

#define REG_GTM_TOM0_CH1_CTRL     (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08040U))
#define REG_GTM_TOM0_CH1_SR0      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08044U))
#define REG_GTM_TOM0_CH1_SR1      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08048U))

#define UPEN_CTRL1                18U
#define HOST_TRIG                 0U
#define ENDIS_CTRL1               2U
#define OUTEN_CTRL1               2U
#define RSTCN0_CH1                18U
#define FUPD_CTRL1                2U
#define CLK_SRC_SR                12U
#define SL                        11U

/* PORT10 - D12 red LED = P10.1 = TOUT103 */
#define PORT10_BASE_ADDRESS       (0xF003B000U)
#define REG_PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10U))
#define PC1                       11U

/* System Timer 0 */
#define STM0_BASE_ADDRESS         (0xF0000000U)
#define REG_STM0_TIM0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))
#define REG_STM0_CMP0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x30U))
#define REG_STM0_CMCON            (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x38U))
#define REG_STM0_ICR              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x3CU))
#define REG_STM0_ISCR             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x40U))

#define CMP0EN                    0U
#define CMP0OS                    2U
#define CMP0IRR                   0U
#define MSIZE0                    0U
#define MSTART0                   8U

/* STM0 Service Request Control */
#define SRC_BASE_ADDRESS          (0xF0038000U)
#define REG_SRC_STM0_SR0          (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0x490U))

#define SRE                       10U
#define TOS                       11U
#define SRPN                      0U

#define STM0_FREQUENCY_HZ         (100000000UL)
#define STM0_TICKS_10MS           (STM0_FREQUENCY_HZ / 100UL)
#define STM0_ISR_PRIORITY         10

/* PWM: 6.25 MHz / 12500 = 500 Hz */
#define PWM_PERIOD_TICKS          12500U
#define PWM_DUTY_STEP             125U

volatile unsigned int g_pwm_duty = 0U;
volatile int g_pwm_direction = 1;     /* 1: brighter, -1: darker */

void init_LED(void);
void init_GTM_TOM0_PWM(void);
void Init_Timer_10ms_Interrupt(void);

IfxCpu_syncEvent cpuSyncEvent = 0;

/*
 * HighTec/AURIX compiler interrupt declaration.
 * No IfxCpu_Irq.h include is required for these compiler keywords.
 */
__interrupt(STM0_ISR_PRIORITY) __vector_table(0)
void Stm0_Compare0_Isr(void)
{
    /* Clear compare interrupt request and schedule the next 10 ms interrupt. */
    REG_STM0_ISCR |= (1U << CMP0IRR);
    REG_STM0_CMP0 += (unsigned int)STM0_TICKS_10MS;

    /* Duty: 0% -> 100% -> 0% repeatedly. */
    if (g_pwm_direction > 0)
    {
        if (g_pwm_duty + PWM_DUTY_STEP >= PWM_PERIOD_TICKS)
        {
            g_pwm_duty = PWM_PERIOD_TICKS;
            g_pwm_direction = -1;
        }
        else
        {
            g_pwm_duty += PWM_DUTY_STEP;
        }
    }
    else
    {
        if (g_pwm_duty <= PWM_DUTY_STEP)
        {
            g_pwm_duty = 0U;
            g_pwm_direction = 1;
        }
        else
        {
            g_pwm_duty -= PWM_DUTY_STEP;
        }
    }

    /* Update PWM duty shadow register and apply it. */
    REG_GTM_TOM0_CH1_SR1 = g_pwm_duty;
    REG_GTM_TOM0_TGC0_GLB_CTRL |= (1U << HOST_TRIG);
}

int core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!! */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();
    init_GTM_TOM0_PWM();
    Init_Timer_10ms_Interrupt();

    while (1)
    {
    }

    return 1;
}

void init_LED(void)
{
    /* P10.1 = Alternate Output Function 1 = GTM TOUT103. */
    REG_PORT10_IOCR0 &= ~((0x1FU) << PC1);
    REG_PORT10_IOCR0 |= ((0x11U) << PC1);
}

void init_GTM_TOM0_PWM(void)
{
    /* Password Access to unlock CPU0 WDT Control Register 0. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U);

    /* Modify Access to clear ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) & ~(1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U);

    /* Enable GTM module. */
    REG_GTM_CLC &= ~(1U << DISR);

    /* Password Access to unlock CPU0 WDT Control Register 0. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U);

    /* Modify Access to set ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U);

    while ((REG_GTM_CLC & (1U << DISS)) != 0U);

    /* GTM Fixed Clock Setting. */
    REG_GTM_CMU_FXCLK_CTRL &= ~((0xFU) << FXCLK_SEL);
    REG_GTM_CMU_CLK_EN |= ((0x2U) << EN_FXCLK);

    /* Allow shadow register update for TOM0 channel 1. */
    REG_GTM_TOM0_TGC0_GLB_CTRL |= ((0x2U) << UPEN_CTRL1);

    REG_GTM_TOM0_TGC0_FUPD_CTRL |= ((0x2U) << FUPD_CTRL1);
    REG_GTM_TOM0_TGC0_FUPD_CTRL |= ((0x2U) << RSTCN0_CH1);

    /* Enable TOM0 channel 1 and its output on the next host trigger. */
    REG_GTM_TOM0_TGC0_ENDIS_CTRL |= ((0x2U) << ENDIS_CTRL1);
    REG_GTM_TOM0_TGC0_OUTEN_CTRL |= ((0x2U) << OUTEN_CTRL1);

    /* PWM active level high. */
    REG_GTM_TOM0_CH1_CTRL |= (1U << SL);

    /* Clock source: CMU_FXCLK1 = 6250 kHz. */
    REG_GTM_TOM0_CH1_CTRL &= ~((0x7U) << CLK_SRC_SR);
    REG_GTM_TOM0_CH1_CTRL |= (1U << CLK_SRC_SR);

    /* 500 Hz PWM, start from 0% duty. */
    REG_GTM_TOM0_CH1_SR0 = PWM_PERIOD_TICKS;
    REG_GTM_TOM0_CH1_SR1 = 0U;

    /* TOUT103 <- TOM0 channel 1. */
    REG_GTM_TOUTSEL6 &= ~((0x3U) << SEL7);

    /* Apply all settings. */
    REG_GTM_TOM0_TGC0_GLB_CTRL |= (1U << HOST_TRIG);
}

void Init_Timer_10ms_Interrupt(void)
{
    unsigned int currentTime;

    /* Disable compare 0 and its service request while configuring. */
    REG_STM0_ICR &= ~(1U << CMP0EN);
    REG_SRC_STM0_SR0 &= ~(1U << SRE);

    /* Compare the full 32-bit TIM0 value. */
    REG_STM0_CMCON &= ~((0x1FU) << MSTART0);
    REG_STM0_CMCON |= ((0x1FU) << MSIZE0);

    /* CMP0 uses compare output 0. */
    REG_STM0_ICR &= ~((0x3U) << CMP0OS);

    /* Clear any pending compare request. */
    REG_STM0_ISCR |= (1U << CMP0IRR);

    currentTime = REG_STM0_TIM0;
    REG_STM0_CMP0 = currentTime + (unsigned int)STM0_TICKS_10MS;

    /* Priority 10, CPU0, service request enabled. */
    REG_SRC_STM0_SR0 &= ~(0xFFU << SRPN);
    REG_SRC_STM0_SR0 |= ((unsigned int)STM0_ISR_PRIORITY << SRPN);

    REG_SRC_STM0_SR0 &= ~(0x3U << TOS);
    REG_SRC_STM0_SR0 |= (1U << SRE);

    REG_STM0_ICR |= (1U << CMP0EN);
}
