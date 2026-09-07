#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

/* Switch 1: Easy Module Shield D2 -> TC275 P02.0 */
#define SW1_PC0               3

/* Red LED: Easy Module Shield D12 -> TC275 P10.1 */
#define RED_PC1               11
#define RED_PS1               1
#define RED_PCL1              17

/* Blue LED: Easy Module Shield D13 -> TC275 P10.2 */
#define BLUE_PC2              19
#define BLUE_PS2              2
#define BLUE_PCL2             18

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

/* 1 = blinking, 0 = stopped. Changed only by the ISR. */
volatile unsigned int blink_enabled = 1U;

/*
 * Interrupt control practice II (slide 161).
 * The slide says to stop/restart blinking with a button interrupt.
 * This file continues from practice I and therefore uses the verified
 * Switch 1 path:
 *   D2 -> P02.0 -> REQ6 -> ERS3/In32 -> ETL3 -> OGU0 -> CPU0.
 */
__interrupt(0x0F) __vector_table(0)
void ISR0(void)
{
    /* Each Switch 1 interrupt alternates blink RUN/STOP. */
    blink_enabled ^= 1U;
}

void init_LEDs(void)
{
    /* P10.1 red LED -> push-pull general-purpose output. */
    PORT10_IOCR0 &= ~((0x1FU) << RED_PC1);
    PORT10_IOCR0 |=  ((0x10U) << RED_PC1);

    /* P10.2 blue LED -> push-pull general-purpose output. */
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

void init_switch(void)
{
    /* P02.0 uses PC0 field in PORT2_IOCR0. */
    PORT2_IOCR0 &= ~((0x1FU) << SW1_PC0);

    /* Set P02.0 to general-purpose input with pull-up. */
    PORT2_IOCR0 |=  ((0x02U) << SW1_PC0);
}

void init_ERU(void)
{
    /*
     * Verified board/MCU path used by the working 05_interrupt_blue.c:
     * P02.0 -> REQ6 -> ERS3 input In32.
     * ERS3 is Input Channel 3, mapped to EICR1 upper-half fields.
     * In32 is input number 2, therefore EXIS1 = 010B.
     */
    SCU_EICR1 &= ~(0x7U << EXIS1);
    SCU_EICR1 |=  (0x2U << EXIS1);

    /* Pull-up switch press: High -> Low, detect falling edge. */
    SCU_EICR1 |=  (0x1U << FEN1);

    /* Enable trigger event generation for ETL3. */
    SCU_EICR1 |=  (0x1U << EIEN1);

    /* Route ETL3 trigger event to OGU0. */
    SCU_EICR1 &= ~(0x7U << INP1);

    /* OGU0: activate IOUT0 whenever a trigger event arrives. */
    SCU_IGCR0 &= ~(0x3U << IGP0);
    SCU_IGCR0 |=  (0x1U << IGP0);

    /* Interrupt priority = 0x0F. */
    SRC_SCU_ERU0 &= ~(0xFFU);
    SRC_SCU_ERU0 |=  (0x0FU);

    /* Enable service request. */
    SRC_SCU_ERU0 |=  (0x1U << SRE);

    /* Route service request to CPU0. */
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

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LEDs();
    init_switch();
    init_ERU();

    while (1)
    {
        /* Slide 161: blink both LEDs continuously while enabled. */
        if (blink_enabled != 0U)
        {
            toggle_red_blue_LEDs();
            delay();
        }
        else
        {
            /* Interrupt has paused blinking; keep current LED states. */
        }
    }
}
