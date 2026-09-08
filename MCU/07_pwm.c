#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

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

/* Generic Timer Module (GTM) - Clock Management Unit(CMU) */
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

/* PORT10 Base Address */
#define PORT10_BASE_ADDRESS    (0xF003B000)
#define PORT10_IOCR0           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PC1                    11

/* Function Prototype */
void init_LED(void);
void init_GTM_TOM0_PWM(void);

IfxCpu_syncEvent cpuSyncEvent = 0;

int core0_main(void)
{
    IfxCpu_enableInterrupts();

    /*
     * !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();
    init_GTM_TOM0_PWM();

    while (1)
    {
    }

    return (1);
}

void init_LED(void)
{
    // Reset PC1 in Port 10 IOCR0 register
    PORT10_IOCR0 &= ~((0x1F) << PC1);

    // Set PC1 to alternative output function1 among push-pull modes in Port 10 IOCR0 register
    PORT10_IOCR0 |= ((0x11) << PC1);
}

void init_GTM_TOM0_PWM(void)
{
    // Password Access to unlock CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) & ~(1 << LCK)) | (1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) != 0);  // Wait until CPU0 WDT Control Register 0 is unlocked

    // Modify Access to clear the ENDINIT bit in CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) | (1 << LCK)) & ~(1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) == 0);  // Wait until the modify access is completed

    // Enable GTM Module
    GTM_CLC &= ~(1 << DISR);

    // Password Access to unlock CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) & ~(1 << LCK)) | (1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) != 0);  // Wait until CPU0 WDT Control Register 0 is unlocked

    // Modify Access to set the ENDINIT bit in CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) | (1 << LCK)) | (1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) == 0);  // Wait until CPU0 WDT Control Register 0 is locked again

    while ((GTM_CLC & (1 << DISS)) != 0);          // Wait until GTM module is enabled

    /* GTM Fixed Clock Setting */
    GTM_CMU_FXCLK_CTRL &= ~((0xF) << FXCLK_SEL);  // Input clock of CMU_FXCLK : CMU_GCLK_EN

    GTM_CMU_CLK_EN |= ((0x2) << EN_FXCLK);        // Enable all CMU_FXCLK

    /*
     * Shadow register update 허용
     */
    GTM_TOM0_TGC0_GLB_CTRL |= ((0x2) << UPEN_CTRL1);      // Enable update of CM0, CM1, CLK_SRC fields in TOM0

    GTM_TOM0_TGC0_FUPD_CTRL |= ((0x2) << FUPD_CTRL1);     // Enable force update of TOM0 channel 1
    GTM_TOM0_TGC0_FUPD_CTRL |= ((0x2) << RSTCN0_CH1);     // Reset CN0 of TOM0 channel 1 on force update

    /*
     * TOM0 CH1 enable/output enable 설정
     * ENDIS_CTRL1 = 0b10 : channel enable request
     * OUTEN_CTRL1 = 0b10 : output enable request
     */
    GTM_TOM0_TGC0_ENDIS_CTRL |= ((0x2) << ENDIS_CTRL1);
    GTM_TOM0_TGC0_OUTEN_CTRL |= ((0x2) << OUTEN_CTRL1);

    /*
     * TOM0 CH1 Control 설정
     *
     * CLK_SRC_SR = 1 : FXCLK1 사용
     * SL         = 1 : PWM active level high
     */
    GTM_TOM0_CH1_CTRL |= (1 << SL);                         // High signal level for duty cycle

    GTM_TOM0_CH1_CTRL &= ~((0x7) << CLK_SRC_SR);           // Clock source : CMU_FXCLK(1) = 6250 kHz
    GTM_TOM0_CH1_CTRL |= (1 << CLK_SRC_SR);

    /*
     * PWM 주기와 듀티 설정
     * CM0 = PWM period
     * CM1 = High 구간 길이
     */
    GTM_TOM0_CH1_SR0 = 12500;                              // PWM freq. = 6250 kHz / 12500 = 500 Hz
    //GTM_TOM0_CH1_SR1 = 6250;                             // Duty cycle = 50 %
    GTM_TOM0_CH1_SR1 = 12500;                              // Duty cycle = 100 %

    GTM_TOUTSEL6 &= ~((0x3) << SEL7);                      // TOUT103 : TOM0 channel 1

    /*
     * Host trigger로 shadow 값을 실제 register에 반영
     */
    GTM_TOM0_TGC0_GLB_CTRL |= (1 << HOST_TRIG);
}
