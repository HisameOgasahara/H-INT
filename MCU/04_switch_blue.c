#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))
#define PORT2_INPUT           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))

/* Port 2 switch input control-field bit positions */
#define SW1_PC0_SHIFT         3
#define SW2_PC1_SHIFT         11

/* Port 10 LED output control-field bit positions */
#define RED_PC1_SHIFT         11
#define BLUE_PC2_SHIFT        19

/* Physical pin bit positions */
#define SW1_PIN               0   /* D2  -> P02.0 */
#define SW2_PIN               1   /* D3  -> P02.1 */
#define RED_LED_PIN           1   /* D12 -> P10.1 */
#define BLUE_LED_PIN          2   /* D13 -> P10.2 */

IfxCpu_syncEvent cpuSyncEvent = 0;

void off_red_LED(void)
{
    PORT10_OUTPUT &= ~(0x1U << RED_LED_PIN);
}

void on_red_LED(void)
{
    PORT10_OUTPUT |= (0x1U << RED_LED_PIN);
}

void off_blue_LED(void)
{
    PORT10_OUTPUT &= ~(0x1U << BLUE_LED_PIN);
}

void on_blue_LED(void)
{
    PORT10_OUTPUT |= (0x1U << BLUE_LED_PIN);
}

void init_LED(void)
{
    /* P10.1 (D12, RED) -> push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1FU) << RED_PC1_SHIFT);
    PORT10_IOCR0 |=  ((0x10U) << RED_PC1_SHIFT);

    /* P10.2 (D13, BLUE) -> push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1FU) << BLUE_PC2_SHIFT);
    PORT10_IOCR0 |=  ((0x10U) << BLUE_PC2_SHIFT);

    off_red_LED();
    off_blue_LED();
}

void init_switch(void)
{
    /* Switch 1: D2 -> P02.0, general-purpose input with pull-up */
    PORT2_IOCR0 &= ~((0x1FU) << SW1_PC0_SHIFT);
    PORT2_IOCR0 |=  ((0x02U) << SW1_PC0_SHIFT);

    /* Switch 2: D3 -> P02.1, general-purpose input with pull-up */
    PORT2_IOCR0 &= ~((0x1FU) << SW2_PC1_SHIFT);
    PORT2_IOCR0 |=  ((0x02U) << SW2_PC1_SHIFT);
}

unsigned int read_switch1(void)
{
    return ((PORT2_INPUT >> SW1_PIN) & 0x1U);
}

unsigned int read_switch2(void)
{
    return ((PORT2_INPUT >> SW2_PIN) & 0x1U);
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
        /* Pull-up inputs: released = 1, pressed = 0 */

        /* Switch 1 (D2) controls RED LED (D12 / P10.1) */
        if (read_switch1() == 0U)
        {
            on_red_LED();
        }
        else
        {
            off_red_LED();
        }

        /* Switch 2 (D3) controls BLUE LED (D13 / P10.2) */
        if (read_switch2() == 0U)
        {
            on_blue_LED();
        }
        else
        {
            off_blue_LED();
        }

        for (int cycle = 0; cycle < 20000000; cycle++);
    }
}
