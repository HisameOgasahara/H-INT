#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))
#define PORT2_INPUT           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

/* P02.0 Switch 1 / P10.2 blue LED */
#define PC0                   3
#define PC2                   19
#define PS2                   2
#define PCL2                  18

/* SCU ERU */
#define SCU_BASE_ADDRESS      (0xF0036000)
#define SCU_EICR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x210))
#define SCU_IGCR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22C))

/* EICR0 upper half: Input Channel 1 = ERS1 / ETL1 */
#define EXIS1                 20
#define FEN1                  24
#define EIEN1                 27
#define INP1                  28
#define IGP0                  14

/* Service Request Control */
#define SRC_BASE_ADDRESS      (0xF0038000)
#define SRC_SCU_ERU0          (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0xCD4))

#define SRE                   10
#define TOS                   11

IfxCpu_syncEvent cpuSyncEvent = 0;

/*
 * Slide 160 practice I:
 * Switch 1 (D2 -> P02.0 -> REQ6 -> ERS1/In10) falling edge
 * toggles LED1, the blue LED (D13 -> P10.2).
 *
 * This is the working 05_interrupt.c structure with only the
 * switch input path and LED pin changed for practice I.
 */
__interrupt(0x0F) __vector_table(0)
void ISR0(void)
{
    /* Toggle P10.2 by setting both PS2 and PCL2 in OMR. */
    PORT10_OMR |= ((0x1U << PCL2) | (0x1U << PS2));
}

void init_LED(void)
{
    /* Reset PC2 in Port 10 IOCR0 register. */
    PORT10_IOCR0 &= ~((0x1FU) << PC2);

    /* Set P10.2 to push-pull general-purpose output. */
    PORT10_IOCR0 |= ((0x10U) << PC2);

    /* Start with LED off. */
    PORT10_OUTPUT &= ~(0x1U << PS2);
}

void init_switch(void)
{
    /* P02.0 uses PC0 field in PORT2_IOCR0. */
    PORT2_IOCR0 &= ~((0x1FU) << PC0);

    /* Set P02.0 to general-purpose input with pull-up. */
    PORT2_IOCR0 |= ((0x02U) << PC0);
}

void init_ERU(void)
{
    /* ERU (External Request Unit) setting. */

    /*
     * Slide 131 input map:
     * P02.0 = REQ6 = ERS1 input In10.
     * Therefore EXIS1 must be 000B (input 0 selected).
     */
    SCU_EICR0 &= ~(0x7U << EXIS1);

    /* Detect falling edge and generate an event. */
    SCU_EICR0 |=  (0x1U << FEN1);
    SCU_EICR0 |=  (0x1U << EIEN1);

    /* Route the event to output channel 0. */
    SCU_EICR0 &= ~(0x7U << INP1);

    /* Activate interrupt output for output channel 0. */
    SCU_IGCR0 &= ~(0x3U << IGP0);
    SCU_IGCR0 |=  (0x1U << IGP0);

    /* SRC (Service Request Control) setting. */
    SRC_SCU_ERU0 &= ~(0xFFU);
    SRC_SCU_ERU0 |=  (0x0FU);

    /* Enable service request generation. */
    SRC_SCU_ERU0 |=  (0x1U << SRE);

    /* Route service request to CPU0. */
    SRC_SCU_ERU0 &= ~(0x3U << TOS);
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();
    init_switch();
    init_ERU();

    while (1)
    {
        /* Main loop stays idle; LED toggling is performed only by the ISR. */
    }
}
