#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * PWM Control Exercise II
 * Easy Module Shield V1 active buzzer:
 *   Arduino D5 -> ShieldBuddy P02.3 -> TOUT3 -> TOM0 channel 11
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

/*
 * TOM0 channel 11 belongs to TGC1 because:
 *   TGC0 -> channels 0..7
 *   TGC1 -> channels 8..15
 * Channel 11 is local channel index 3 inside TGC1.
 */
#define REG_GTM_TOM0_TGC1_GLB_CTRL    (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08230U))
#define REG_GTM_TOM0_TGC1_FUPD_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08238U))
#define REG_GTM_TOM0_TGC1_ENDIS_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08270U))
#define REG_GTM_TOM0_TGC1_OUTEN_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08278U))

#define REG_GTM_TOM0_CH11_CTRL         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C0U))
#define REG_GTM_TOM0_CH11_SR0          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C4U))
#define REG_GTM_TOM0_CH11_SR1          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C8U))

/* TGC1 local channel 3 field positions */
#define UPEN_CTRL3                22U
#define HOST_TRIG                 0U
#define ENDIS_CTRL3               6U
#define OUTEN_CTRL3               6U
#define FUPD_CTRL3                6U
#define RSTCN0_CH3                22U

#define CLK_SRC_SR                12U
#define SL                        11U

/* PORT2: P02.3 is the buzzer output pin */
#define PORT2_BASE_ADDRESS        (0xF003A200U)
#define REG_PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10U))
#define PC3                       27U

/*
 * CMU_FXCLK1 = 6.25 MHz, same clock selection used in 07_pwm.c.
 * 6.25 MHz / 6250 = 1 kHz PWM.
 * SR1 = 3125 gives 50% duty cycle.
 */
#define BUZZER_PWM_PERIOD         6250U
#define BUZZER_PWM_DUTY           3125U

void init_Buzzer(void);
void init_GTM_TOM0_Buzzer_PWM(void);

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

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_Buzzer();
    init_GTM_TOM0_Buzzer_PWM();

    while (1)
    {
    }

    return 1;
}

void init_Buzzer(void)
{
    /* Clear PC3 field of P02.3. */
    REG_PORT2_IOCR0 &= ~((0x1FU) << PC3);

    /* P02.3 = push-pull Alternate Output Function 1 = GTM TOUT3. */
    REG_PORT2_IOCR0 |= ((0x11U) << PC3);
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

    /* Enable fixed clocks; select CMU_GCLK_EN as the FXCLK input. */
    REG_GTM_CMU_FXCLK_CTRL &= ~((0xFU) << FXCLK_SEL);
    REG_GTM_CMU_CLK_EN |= ((0x2U) << EN_FXCLK);

    /*
     * Channel 11 is local channel 3 of TGC1.
     * Allow CM0, CM1 and CLK_SRC to be updated from the shadow registers.
     */
    REG_GTM_TOM0_TGC1_GLB_CTRL |= ((0x2U) << UPEN_CTRL3);

    /* Force update and reset CN0 for local channel 3. */
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= ((0x2U) << FUPD_CTRL3);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= ((0x2U) << RSTCN0_CH3);

    /* Enable TOM0 channel 11 and its output on the host trigger. */
    REG_GTM_TOM0_TGC1_ENDIS_CTRL |= ((0x2U) << ENDIS_CTRL3);
    REG_GTM_TOM0_TGC1_OUTEN_CTRL |= ((0x2U) << OUTEN_CTRL3);

    /* Active level high. */
    REG_GTM_TOM0_CH11_CTRL |= (1U << SL);

    /* Use CMU_FXCLK1 = 6.25 MHz. */
    REG_GTM_TOM0_CH11_CTRL &= ~((0x7U) << CLK_SRC_SR);
    REG_GTM_TOM0_CH11_CTRL |= (1U << CLK_SRC_SR);

    /* 1 kHz, 50% duty PWM for the buzzer. */
    REG_GTM_TOM0_CH11_SR0 = BUZZER_PWM_PERIOD;
    REG_GTM_TOM0_CH11_SR1 = BUZZER_PWM_DUTY;

    /*
     * P02.3 is TOUT3. In the GTM port mapping, Timer A for TOUT3 is TOM0_11.
     * Therefore TOUTSEL0.SEL3 = 00B.
     */
    REG_GTM_TOUTSEL0 &= ~((0x3U) << SEL3);

    /* Apply ENDIS/OUTEN and shadow-register settings. */
    REG_GTM_TOM0_TGC1_GLB_CTRL |= (1U << HOST_TRIG);
}
