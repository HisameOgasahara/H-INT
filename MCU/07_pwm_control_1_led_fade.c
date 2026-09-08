#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxCpu_Irq.h"

/* System Control Unit */
#define SCU_BASE_ADDRESS       (0xF0036000)
#define SCU_WDTCPU0CON0        (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100))

#define LCK                    1
#define ENDINIT                0

/* Generic Timer Module (GTM) Control Registers */
#define GTM_BASE_ADDRESS       (0xF0100000)
#define GTM_CLC                (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00))
#define GTM_TOUTSEL6           (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD48))

#define DISS                   1
#define DISR                   0
#define SEL7                   14

/* Generic Timer Module (GTM) - Clock Management Unit (CMU) */
#define GTM_CMU_CLK_EN         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300))
#define GTM_CMU_FXCLK_CTRL     (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344))

#define EN_FXCLK               22
#define FXCLK_SEL              0

/* Generic Timer Module (GTM) - Timer Output Module (TOM) */
#define GTM_TOM0_TGC0_GLB_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08030))
#define GTM_TOM0_TGC0_ENDIS_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08070))
#define GTM_TOM0_TGC0_OUTEN_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08078))
#define GTM_TOM0_TGC0_FUPD_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08038))

#define GTM_TOM0_CH1_CTRL      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08040))
#define GTM_TOM0_CH1_SR0       (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08044))
#define GTM_TOM0_CH1_SR1       (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08048))

#define UPEN_CTRL1             18
#define HOST_TRIG              0
#define ENDIS_CTRL1            2
#define OUTEN_CTRL1            2
#define RSTCN0_CH1             18
#define FUPD_CTRL1             2
#define CLK_SRC_SR             12
#define SL                     11

/* PORT10 - D12 red LED = P10.1 = TOUT103 */
#define PORT10_BASE_ADDRESS    (0xF003B000)
#define PORT10_IOCR0           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PC1                    11

/* System Timer 0 */
#define STM0_BASE_ADDRESS      (0xF0000000)
#define STM0_TIM0              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10))
#define STM0_CMP0              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x30))
#define STM0_CMCON             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x38))
#define STM0_ICR               (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x3C))
#define STM0_ISCR              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x40))

#define CMP0EN                 0
#define CMP0OS                 2
#define CMP0IRR                0
#define MSIZE0                 0
#define MSTART0                8

/* STM0 Service Request Control */
#define SRC_BASE_ADDRESS       (0xF0038000)
#define SRC_STM0_SR0           (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0x490))

#define SRE                    10
#define TOS                    11
#define SRPN                   0

#define STM0_FREQUENCY_HZ      (100000000UL)
#define STM0_TICKS_10MS        (STM0_FREQUENCY_HZ / 100UL)
#define STM0_ISR_PRIORITY      10

/* PWM: 6.25 MHz / 12500 = 500 Hz */
#define PWM_PERIOD_TICKS       12500U
#define PWM_DUTY_STEP          125U

volatile unsigned int g_pwm_duty = 0U;
volatile int g_pwm_direction = 1;     /* 1: brighter, -1: darker */

void init_LED(void);
void init_GTM_TOM0_PWM(void);
void Init_Timer_10ms_Interrupt(void);

IfxCpu_syncEvent cpuSyncEvent = 0;

__interrupt(STM0_ISR_PRIORITY) __vector_table(0)
void Stm0_Compare0_Isr(void)
{
    /* Clear compare interrupt request and schedule the next 10 ms interrupt. */
    STM0_ISCR |= (1U << CMP0IRR);
    STM0_CMP0 += (unsigned int)STM0_TICKS_10MS;

    /*
     * PWM control exercise I:
     * change SR1 from 0 -> 12500 -> 0 repeatedly.
     * With SR0 fixed at 12500 this changes duty cycle from 0% -> 100% -> 0%.
     */
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
    GTM_TOM0_CH1_SR1 = g_pwm_duty;
    GTM_TOM0_TGC0_GLB_CTRL |= (1U << HOST_TRIG);
}

int core0_main(void)
{
    IfxCpu_enableInterrupts();

    /*
     * !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required.
     */
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
    /* Reset PC1 in Port 10 IOCR0 register. */
    PORT10_IOCR0 &= ~((0x1FU) << PC1);

    /* P10.1 = Alternate Output Function 1 = GTM TOUT103. */
    PORT10_IOCR0 |= ((0x11U) << PC1);
}

void init_GTM_TOM0_PWM(void)
{
    /* Password Access to unlock CPU0 WDT Control Register 0. */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1U << LCK)) != 0U);

    /* Modify Access to clear ENDINIT. */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) & ~(1U << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1U << LCK)) == 0U);

    /* Enable GTM module. */
    GTM_CLC &= ~(1U << DISR);

    /* Password Access to unlock CPU0 WDT Control Register 0. */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1U << LCK)) != 0U);

    /* Modify Access to set ENDINIT. */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) | (1U << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1U << LCK)) == 0U);

    while ((GTM_CLC & (1U << DISS)) != 0U);

    /* GTM Fixed Clock Setting. */
    GTM_CMU_FXCLK_CTRL &= ~((0xFU) << FXCLK_SEL);
    GTM_CMU_CLK_EN |= ((0x2U) << EN_FXCLK);

    /* Allow shadow register update for TOM0 channel 1. */
    GTM_TOM0_TGC0_GLB_CTRL |= ((0x2U) << UPEN_CTRL1);

    GTM_TOM0_TGC0_FUPD_CTRL |= ((0x2U) << FUPD_CTRL1);
    GTM_TOM0_TGC0_FUPD_CTRL |= ((0x2U) << RSTCN0_CH1);

    /* Enable TOM0 channel 1 and its output on the next host trigger. */
    GTM_TOM0_TGC0_ENDIS_CTRL |= ((0x2U) << ENDIS_CTRL1);
    GTM_TOM0_TGC0_OUTEN_CTRL |= ((0x2U) << OUTEN_CTRL1);

    /* PWM active level high. */
    GTM_TOM0_CH1_CTRL |= (1U << SL);

    /* Clock source: CMU_FXCLK1 = 6250 kHz. */
    GTM_TOM0_CH1_CTRL &= ~((0x7U) << CLK_SRC_SR);
    GTM_TOM0_CH1_CTRL |= (1U << CLK_SRC_SR);

    /* 500 Hz PWM, start from 0% duty. */
    GTM_TOM0_CH1_SR0 = PWM_PERIOD_TICKS;
    GTM_TOM0_CH1_SR1 = 0U;

    /* TOUT103 <- TOM0 channel 1. */
    GTM_TOUTSEL6 &= ~((0x3U) << SEL7);

    /* Apply all settings. */
    GTM_TOM0_TGC0_GLB_CTRL |= (1U << HOST_TRIG);
}

void Init_Timer_10ms_Interrupt(void)
{
    unsigned int currentTime;

    /* Disable compare 0 and its service request while configuring. */
    STM0_ICR &= ~(1U << CMP0EN);
    SRC_STM0_SR0 &= ~(1U << SRE);

    /* Compare the full 32-bit TIM0 value. */
    STM0_CMCON &= ~((0x1FU) << MSTART0);
    STM0_CMCON |= ((0x1FU) << MSIZE0);

    /* CMP0 uses compare output 0. */
    STM0_ICR &= ~((0x3U) << CMP0OS);

    /* Clear any pending compare request. */
    STM0_ISCR |= (1U << CMP0IRR);

    currentTime = STM0_TIM0;
    STM0_CMP0 = currentTime + (unsigned int)STM0_TICKS_10MS;

    /* Priority 10, CPU0, service request enabled. */
    SRC_STM0_SR0 &= ~(0xFFU << SRPN);
    SRC_STM0_SR0 |= ((unsigned int)STM0_ISR_PRIORITY << SRPN);

    SRC_STM0_SR0 &= ~(0x3U << TOS);
    SRC_STM0_SR0 |= (1U << SRE);

    STM0_ICR |= (1U << CMP0EN);
}
