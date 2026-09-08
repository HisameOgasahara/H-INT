#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * PWM Control 3
 *
 * Easy Module Shield V1:
 *   SW1          : Arduino D2  -> ShieldBuddy TC275 P02.0
 *   Active buzzer: Arduino D5  -> ShieldBuddy TC275 P02.3
 *
 * Operation:
 *   SW1 pressed  -> 1 kHz, 50% PWM output to buzzer
 *   SW1 released -> 0% duty, buzzer off
 */

/* System Control Unit */
#define SCU_BASE_ADDRESS          (0xF0036000U)
#define REG_SCU_WDTCPU0CON0       (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100U))

#define LCK                       1U
#define ENDINIT                   0U

/* Generic Timer Module (GTM) */
#define GTM_BASE_ADDRESS          (0xF0100000U)
#define REG_GTM_CLC               (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00U))
#define REG_GTM_TOUTSEL0          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD30U))

#define DISS                      1U
#define DISR                      0U
#define SEL3                      6U

/* GTM Clock Management Unit (CMU) */
#define REG_GTM_CMU_CLK_EN        (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300U))
#define REG_GTM_CMU_FXCLK_CTRL    (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344U))

#define EN_FXCLK                  22U
#define FXCLK_SEL                 0U

/* TOM0 channel 11 -> TGC1 local channel 3 */
#define REG_GTM_TOM0_TGC1_GLB_CTRL    (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08230U))
#define REG_GTM_TOM0_TGC1_FUPD_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08238U))
#define REG_GTM_TOM0_TGC1_ENDIS_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08270U))
#define REG_GTM_TOM0_TGC1_OUTEN_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08278U))

#define REG_GTM_TOM0_CH11_CTRL         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C0U))
#define REG_GTM_TOM0_CH11_SR0          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C4U))
#define REG_GTM_TOM0_CH11_SR1          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C8U))
#define REG_GTM_TOM0_CH11_CM1          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082D0U))

#define UPEN_CTRL3                22U
#define HOST_TRIG                 0U
#define ENDIS_CTRL3               6U
#define OUTEN_CTRL3               6U
#define FUPD_CTRL3                6U
#define RSTCN0_CH3                22U

#define CLK_SRC_SR                12U
#define SL                        11U

/* PORT2: SW1=P02.0, buzzer=P02.3 */
#define PORT2_BASE_ADDRESS        (0xF003A200U)
#define REG_PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10U))
#define REG_PORT2_INPUT           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24U))

#define PC0                       3U
#define PC3                       27U
#define P0                        0U

/* 6.25 MHz / 6250 = 1 kHz, 50% duty = 3125 */
#define BUZZER_PWM_PERIOD         6250U
#define BUZZER_PWM_DUTY           3125U

void init_switch(void);
unsigned int read_switch(void);
void init_Buzzer(void);
void init_GTM_TOM0_Buzzer_PWM(void);
void buzzer_on(void);
void buzzer_off(void);

IfxCpu_syncEvent cpuSyncEvent = 0;

int core0_main(void)
{
    IfxCpu_enableInterrupts();

    /*
     * !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable and service them periodically when required by the application.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_switch();
    init_Buzzer();
    init_GTM_TOM0_Buzzer_PWM();

    /* Start silent. */
    buzzer_off();

    while (1)
    {
        /*
         * SW1 uses a pull-up input:
         *   released = 1
         *   pressed  = 0
         */
        if (read_switch() == 0U)
        {
            buzzer_on();
        }
        else
        {
            buzzer_off();
        }
    }

    return 1;
}

void init_switch(void)
{
    /* P02.0: general-purpose input with pull-up. */
    REG_PORT2_IOCR0 &= ~((0x1FU) << PC0);
    REG_PORT2_IOCR0 |=  ((0x02U) << PC0);
}

unsigned int read_switch(void)
{
    /* SW1: D2 -> P02.0 */
    return ((REG_PORT2_INPUT >> P0) & 0x1U);
}

void init_Buzzer(void)
{
    /* P02.3 = Alternate Output Function 1 = GTM TOUT3. */
    REG_PORT2_IOCR0 &= ~((0x1FU) << PC3);
    REG_PORT2_IOCR0 |=  ((0x11U) << PC3);
}

void buzzer_on(void)
{
    /*
     * TC27x GTM TOM asynchronous duty update:
     * write SR1 and CM1 with the same value so the next synchronous
     * shadow update cannot overwrite the directly written CM1 value.
     */
    REG_GTM_TOM0_CH11_SR1 = BUZZER_PWM_DUTY;
    REG_GTM_TOM0_CH11_CM1 = BUZZER_PWM_DUTY;
}

void buzzer_off(void)
{
    /* CM1 = 0 gives 0% duty cycle. Keep SR1 identical to CM1. */
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOM0_CH11_CM1 = 0U;
}

void init_GTM_TOM0_Buzzer_PWM(void)
{
    /* Password access: unlock CPU0 WDT Control Register 0. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U)
    {
    }

    /* Modify access: clear ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) & ~(1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U)
    {
    }

    /* Enable GTM module. */
    REG_GTM_CLC &= ~(1U << DISR);

    /* Password access: unlock CPU0 WDT Control Register 0. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U)
    {
    }

    /* Modify access: set ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U)
    {
    }

    while ((REG_GTM_CLC & (1U << DISS)) != 0U)
    {
    }

    /* Enable fixed clocks and select CMU_GCLK_EN as FXCLK input. */
    REG_GTM_CMU_FXCLK_CTRL &= ~((0xFU) << FXCLK_SEL);
    REG_GTM_CMU_CLK_EN |= ((0x2U) << EN_FXCLK);

    /* TOM0 channel 11 = local channel 3 in TGC1. */
    REG_GTM_TOM0_TGC1_GLB_CTRL |= ((0x2U) << UPEN_CTRL3);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= ((0x2U) << FUPD_CTRL3);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= ((0x2U) << RSTCN0_CH3);

    /* Enable channel and output. */
    REG_GTM_TOM0_TGC1_ENDIS_CTRL |= ((0x2U) << ENDIS_CTRL3);
    REG_GTM_TOM0_TGC1_OUTEN_CTRL |= ((0x2U) << OUTEN_CTRL3);

    /* Active level high. */
    REG_GTM_TOM0_CH11_CTRL |= (1U << SL);

    /* Use CMU_FXCLK1 = 6.25 MHz. */
    REG_GTM_TOM0_CH11_CTRL &= ~((0x7U) << CLK_SRC_SR);
    REG_GTM_TOM0_CH11_CTRL |= (1U << CLK_SRC_SR);

    /* 1 kHz PWM, initially silent (0% duty). */
    REG_GTM_TOM0_CH11_SR0 = BUZZER_PWM_PERIOD;
    REG_GTM_TOM0_CH11_SR1 = 0U;

    /* TOUT3 <- TOM0 channel 11. */
    REG_GTM_TOUTSEL0 &= ~((0x3U) << SEL3);

    /* Apply initial channel/output/shadow settings. */
    REG_GTM_TOM0_TGC1_GLB_CTRL |= (1U << HOST_TRIG);
}
