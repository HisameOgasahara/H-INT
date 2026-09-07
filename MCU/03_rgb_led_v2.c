#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS   (0xF003A200)
#define PORT2_IOCR4          (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x14))
#define PORT2_OMR            (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x04))

#define PORT10_BASE_ADDRESS  (0xF003B000)
#define PORT10_IOCR0         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_IOCR4         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x14))
#define PORT10_OMR           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

#define PC3   27
#define PC5   11
#define PC7   27

#define PS3    3
#define PS5    5
#define PS7    7

#define PCL3  19
#define PCL5  21
#define PCL7  23

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_RGBLED(void)
{
    // RED LED: P02.7 -> push-pull general-purpose output
    PORT2_IOCR4 &= ~((0x1F) << PC7);
    PORT2_IOCR4 |=  ((0x10) << PC7);

    // GREEN LED: P10.5 -> push-pull general-purpose output
    PORT10_IOCR4 &= ~((0x1F) << PC5);
    PORT10_IOCR4 |=  ((0x10) << PC5);

    // BLUE LED: P10.3 -> push-pull general-purpose output
    PORT10_IOCR0 &= ~((0x1F) << PC3);
    PORT10_IOCR0 |=  ((0x10) << PC3);
}

void blink_RGBLED(void)
{
    // OMR rule: PCLx = 1 and PSx = 1 -> toggle Pn_OUT.Px
    // Toggle BLUE(P10.3) and GREEN(P10.5) in one write.
    PORT10_OMR = ((0x1U << PCL3) | (0x1U << PS3) |
                  (0x1U << PCL5) | (0x1U << PS5));

    // Toggle RED(P02.7).
    PORT2_OMR = ((0x1U << PCL7) | (0x1U << PS7));
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_RGBLED();

    while (1)
    {
        blink_RGBLED();

        for (int cycle = 0; cycle < 20000000; cycle++);
    }
}
