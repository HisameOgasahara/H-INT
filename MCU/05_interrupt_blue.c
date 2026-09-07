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

/* P02.0 = Switch 1, P10.2 = blue LED */
#define PC0                   3
#define PC2                   19
#define PS2                   2
#define PCL2                  18

/* SCU ERU */
#define SCU_BASE_ADDRESS      (0xF0036000)
#define SCU_EICR1             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x214))
#define SCU_IGCR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22C))

/* EICR1 upper half controls Input Channel 3 (ERS3 / ETL3). */
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
 * Interrupt control practice I (slide 160).
 * Starting from 05_interrupt.c:
 *   Switch 2 (P02.1 / REQ14 / ERS2-In21) -> Switch 1 (P02.0 / REQ6 / ERS3-In32)
 *   Red LED (P10.1) -> Blue LED (P10.2)
 * OGU0, SRC_SCU_ERU0, priority, CPU0 and ISR structure are unchanged.
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
     * Slide 131 ERU input map:
     * P02.0 -> REQ6 -> ERS3 input In32.
     * ERS3 is Input Channel 3, mapped to EICR1 upper-half fields.
     * In32 is input number 2, therefore EXIS1 = 010B.
     */
    SCU_EICR1 &= ~(0x7U << EXIS1);
    SCU_EICR1 |=  (0x2U << EXIS1);

    /* Detect falling edge and generate an event. */
    SCU_EICR1 |=  (0x1U << FEN1);
    SCU_EICR1 |=  (0x1U << EIEN1);

    /* Route the event to output channel 0. */
    SCU_EICR1 &= ~(0x7U << INP1);

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
