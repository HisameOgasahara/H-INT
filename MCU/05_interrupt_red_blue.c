#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

/* Switch 2: Shield D3 -> TC275 P02.1 */
#define SW2_PC1               11

/* Red LED: Shield D12 -> TC275 P10.1 */
#define RED_PC1               11
#define RED_PS1               1
#define RED_PCL1              17

/* Blue LED: Shield D13 -> TC275 P10.2 */
#define BLUE_PC2              19
#define BLUE_PS2              2
#define BLUE_PCL2             18

/* SCU ERU */
#define SCU_BASE_ADDRESS      (0xF0036000)
#define SCU_EICR1             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x214))
#define SCU_IGCR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22C))

#define EXIS0                 4
#define FEN0                  8
#define EIEN0                 11
#define INP0                  12
#define IGP0                  14

/* Service Request Control */
#define SRC_BASE_ADDRESS      (0xF0038000)
#define SRC_SCU_ERU0          (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0xCD4))

#define SRE                   10
#define TOS                   11

IfxCpu_syncEvent cpuSyncEvent = 0;

/* 1 = blinking, 0 = stopped. Changed only by the ISR. */
volatile unsigned int blink_enabled = 1U;

/*
 * Interrupt control practice II (slide 161)
 * Each button press alternates between blink and pause.
 */
__interrupt(0x0F) __vector_table(0)
void ISR0(void)
{
    blink_enabled ^= 1U;
}

void init_LEDs(void)
{
    /* P10.1 red LED -> push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1FU) << RED_PC1);
    PORT10_IOCR0 |=  ((0x10U) << RED_PC1);

    /* P10.2 blue LED -> push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1FU) << BLUE_PC2);
    PORT10_IOCR0 |=  ((0x10U) << BLUE_PC2);

    /* Both LEDs start OFF. */
    PORT10_OUTPUT &= ~((0x1U << RED_PS1) | (0x1U << BLUE_PS2));
}

void toggle_red_blue_LEDs(void)
{
    /* Toggle P10.1 and P10.2 in one OMR write. */
    PORT10_OMR = ((0x1U << RED_PCL1)  | (0x1U << RED_PS1) |
                  (0x1U << BLUE_PCL2) | (0x1U << BLUE_PS2));
}

void init_switch2(void)
{
    /* P02.1 -> general-purpose input with pull-up */
    PORT2_IOCR0 &= ~((0x1FU) << SW2_PC1);
    PORT2_IOCR0 |=  ((0x02U) << SW2_PC1);
}

void init_ERU_switch2(void)
{
    /* P02.1 -> REQ14 -> ERS2 input 1, therefore EXIS0 = 001B. */
    SCU_EICR1 &= ~(0x7U << EXIS0);
    SCU_EICR1 |=  (0x1U << EXIS0);

    /* Button press produces a falling edge. */
    SCU_EICR1 |=  (0x1U << FEN0);

    /* Enable trigger event generation. */
    SCU_EICR1 |=  (0x1U << EIEN0);

    /* Route ETL2 trigger to OGU0. */
    SCU_EICR1 &= ~(0x7U << INP0);

    /* OGU0: activate IOUT0 whenever trigger arrives. */
    SCU_IGCR0 &= ~(0x3U << IGP0);
    SCU_IGCR0 |=  (0x1U << IGP0);

    /* Interrupt priority = 0x0F. */
    SRC_SCU_ERU0 &= ~(0xFFU);
    SRC_SCU_ERU0 |=  (0x0FU);

    /* Enable service request and route it to CPU0. */
    SRC_SCU_ERU0 |=  (0x1U << SRE);
    SRC_SCU_ERU0 &= ~(0x3U << TOS);
}

void delay(void)
{
    volatile unsigned int cycle;
    for (cycle = 0U; cycle < 20000000U; cycle++)
    {
        /* Busy wait. Interrupts remain enabled during this delay. */
    }
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LEDs();
    init_switch2();
    init_ERU_switch2();

    while (1)
    {
        if (blink_enabled != 0U)
        {
            toggle_red_blue_LEDs();
            delay();
        }
        else
        {
            /* Pause: keep the current LED states unchanged until next interrupt. */
        }
    }
}
