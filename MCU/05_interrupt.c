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

/* P10.1 red LED */
#define PC1                   11
#define PS1                   1
#define PCL1                  17

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

/*
 * ERU0 interrupt service routine.
 * Priority 0x0F, vector table 0 = CPU0.
 * Switch 2 (Shield D3 -> TC275 P02.1) falling edge toggles P10.1 red LED.
 */
__interrupt(0x0F) __vector_table(0)
void ISR0(void)
{
    /* Toggle P10.1 by setting both PS1 and PCL1 in OMR. */
    PORT10_OMR |= ((0x1U << PCL1) | (0x1U << PS1));
}

void init_LED(void)
{
    /* Reset PC1 in Port 10 IOCR0 register. */
    PORT10_IOCR0 &= ~((0x1FU) << PC1);

    /* Set P10.1 to push-pull general-purpose output. */
    PORT10_IOCR0 |= ((0x10U) << PC1);

    /* Start with LED off. */
    PORT10_OUTPUT &= ~(0x1U << PS1);
}

void init_switch(void)
{
    /* P02.1 uses PC1 field in PORT2_IOCR0. */
    PORT2_IOCR0 &= ~((0x1FU) << PC1);

    /* Set P02.1 to general-purpose input with pull-up. */
    PORT2_IOCR0 |= ((0x02U) << PC1);
}

void init_ERU(void)
{
    /* ERU (External Request Unit) setting. */

    /* Select P02.1 / REQ14 as ERU input source. */
    SCU_EICR1 &= ~(0x7U << EXIS0);
    SCU_EICR1 |=  (0x1U << EXIS0);

    /* Detect falling edge and generate an event. */
    SCU_EICR1 |=  (0x1U << FEN0);
    SCU_EICR1 |=  (0x1U << EIEN0);

    /* Route the event to output channel 0. */
    SCU_EICR1 &= ~(0x7U << INP0);

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
