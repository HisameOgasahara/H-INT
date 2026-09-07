#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT10_BASE_ADDRESS   (0xF003B000)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x4))

/* P10.1 = RED LED */
#define PC1   11
#define PS1   1
#define PCL1  17

/* P10.2 = BLUE LED */
#define PC2   19
#define PS2   2
#define PCL2  18

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_LED(void)
{
    /* P10.1 -> Push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1F) << PC1);
    PORT10_IOCR0 |=  ((0x10) << PC1);

    /* P10.2 -> Push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1F) << PC2);
    PORT10_IOCR0 |=  ((0x10) << PC2);
}

void set_LED_state(int state)
{
    switch (state)
    {
        case 0:
            /* RED OFF, BLUE OFF */
            PORT10_OMR =
                (1 << PCL1) |
                (1 << PCL2);
            break;

        case 1:
            /* RED ON, BLUE OFF */
            PORT10_OMR =
                (1 << PS1)  |
                (1 << PCL2);
            break;

        case 2:
            /* RED OFF, BLUE ON */
            PORT10_OMR =
                (1 << PCL1) |
                (1 << PS2);
            break;

        case 3:
            /* RED ON, BLUE ON */
            PORT10_OMR =
                (1 << PS1) |
                (1 << PS2);
            break;
    }
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(
        IfxScuWdt_getCpuWatchdogPassword());

    IfxScuWdt_disableSafetyWatchdog(
        IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();

    while (1)
    {
        for (int state = 0; state < 4; state++)
        {
            set_LED_state(state);

            for (int cycle = 0; cycle < 20000000; cycle++);
        }
    }
}