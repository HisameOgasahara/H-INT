#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))
#define PORT2_INPUT           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))

#define PC0   3
#define PC1   11
#define P1    1

IfxCpu_syncEvent cpuSyncEvent = 0;

void off_LED(void)
{
    /* P10.1 Low */
    PORT10_OUTPUT &= ~(0x1U << P1);
}

void on_LED(void)
{
    /* P10.1 High */
    PORT10_OUTPUT |= (0x1U << P1);
}

void init_LED(void)
{
    /* Reset PC1 in Port 10 IOCR0 register */
    PORT10_IOCR0 &= ~((0x1FU) << PC1);

    /* Set P10.1 to push-pull general-purpose output */
    PORT10_IOCR0 |= ((0x10U) << PC1);

    /* Start with LED off */
    off_LED();
}

void init_switch(void)
{
    /* Reset PC0 in Port 2 IOCR0 register */
    PORT2_IOCR0 &= ~((0x1FU) << PC0);

    /* Set P02.0 to general-purpose input with pull-up */
    PORT2_IOCR0 |= ((0x02U) << PC0);
}

unsigned int read_switch(void)
{
    /* Switch 1: Shield D2 -> TC275 P02.0 */
    return (PORT2_INPUT & 0x1U);
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

    init_LED();
    init_switch();

    while (1)
    {
        /* Pull-up input: released = 1, pressed = 0 */
        if (read_switch() == 1U)
        {
            off_LED();
        }
        else
        {
            on_LED();
        }

        for (int cycle = 0; cycle < 20000000; cycle++);
    }
}
