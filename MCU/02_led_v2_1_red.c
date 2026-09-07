#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT10_BASE_ADDRESS   (0xF003B000)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x4))

#define PC1   11
#define PS1   1
#define PCL1  17

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_LED(void)
{
    // Reset PC1 in Port 10 IOCR0 register
    PORT10_IOCR0 &= ~((0x1F) << PC1);

    // Set PC1 to push-pull mode in Port 10 IOCR0 register
    PORT10_IOCR0 |= ((0x10) << PC1);
}

void blink_LED(void)
{
    // Set 1 to PCL1 and PS1 in Port 10 output modification register for LED toggle
    PORT10_OMR |= ((0x1 << PCL1) | (0x1 << PS1));
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();

    while(1)
    {
        blink_LED();
        for(int cycle = 0; cycle < 20000000; cycle++);
    }
}