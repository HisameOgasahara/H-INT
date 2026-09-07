#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT10_BASE_ADDRESS   (0xF003B000)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x4))

#define PC2   19
#define PS2   2
#define PCL2  18

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_LED(void)
{
    PORT10_IOCR0 &= ~((0x1F) << PC2);
    PORT10_IOCR0 |=  ((0x10) << PC2);
}

void blink_LED(void)
{
    PORT10_OMR |= ((0x1 << PCL2) | (0x1 << PS2));
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