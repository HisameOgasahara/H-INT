#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT2_IOCR4           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x14))
#define PORT2_OUTPUT          (*(volatile unsigned int *)(PORT2_BASE_ADDRESS))

#define PORT10_BASE_ADDRESS   (0xF003B000)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_IOCR4          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x14))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))

#define PC3   27
#define PC5   11
#define PC7   27

#define P3    3
#define P5    5
#define P7    7

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_RGBLED(void)
{
    // Reset PC7 in Port 2 IOCR4 register
    PORT2_IOCR4 &= ~((0x1F) << PC7);

    // Set PC7 to push-pull mode in Port 2 IOCR4 register
    PORT2_IOCR4 |= ((0x10) << PC7);

    // Reset PC5 in Port 10 IOCR4 register
    PORT10_IOCR4 &= ~((0x1F) << PC5);

    // Set PC5 to push-pull mode in Port 10 IOCR4 register
    PORT10_IOCR4 |= ((0x10) << PC5);

    // Reset PC3 in Port 10 IOCR0 register
    PORT10_IOCR0 &= ~((0x1F) << PC3);

    // Set PC3 to push-pull mode in Port 10 IOCR0 register
    PORT10_IOCR0 |= ((0x10) << PC3);
}

void blink_RGBLED(void)
{
    static int LED_flag = 1;

    if (LED_flag == 1)
    {
        // Set P3 in Port 10 output register for BLUE LED On
        PORT10_OUTPUT |= (0x1 << P3);

        // Set P5 in Port 10 output register for GREEN LED On
        PORT10_OUTPUT |= (0x1 << P5);

        // Set P7 in Port 2 output register for RED LED On
        PORT2_OUTPUT |= (0x1 << P7);

        LED_flag = 0;
    }
    else
    {
        // Clear P3, P5, and P7 for RGB LED Off
        PORT10_OUTPUT &= ~(0x1 << P3);
        PORT10_OUTPUT &= ~(0x1 << P5);
        PORT2_OUTPUT &= ~(0x1 << P7);

        LED_flag = 1;
    }
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

    init_RGBLED();

    while(1)
    {
        blink_RGBLED();

        for(int cycle = 0; cycle < 20000000; cycle++);
    }
}
