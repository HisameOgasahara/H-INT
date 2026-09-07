#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

/* Switch 1: Easy Module Shield D2 -> ShieldBuddy digital 2 -> TC275 P02.0 */
#define SW1_PC0               3

/* Blue LED: Easy Module Shield D13 -> ShieldBuddy digital 13 -> TC275 P10.2 */
#define BLUE_PC2              19
#define BLUE_PS2              2
#define BLUE_PCL2             18

/* SCU ERU */
#define SCU_BASE_ADDRESS      (0xF0036000)
#define SCU_EICR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x210))
#define SCU_IGCR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22C))

/* EICR0 upper half controls Input Channel 1 (ERS1 / ETL1). */
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
 * Interrupt control practice I (slide 160)
 * Switch 1 (D2 / P02.0) falling edge toggles LED1 (blue, D13 / P10.2).
 */
__interrupt(0x0F) __vector_table(0)
void ISR0(void)
{
    /* P10.2 toggle: PCL2=1 and PS2=1 in OMR. */
    PORT10_OMR = ((0x1U << BLUE_PCL2) | (0x1U << BLUE_PS2));
}

void init_blue_LED(void)
{
    /* P10.2 -> push-pull general-purpose output. */
    PORT10_IOCR0 &= ~((0x1FU) << BLUE_PC2);
    PORT10_IOCR0 |=  ((0x10U) << BLUE_PC2);

    /* Start with blue LED OFF. */
    PORT10_OUTPUT &= ~(0x1U << BLUE_PS2);
}

void init_switch1(void)
{
    /* P02.0 -> general-purpose input with pull-up. */
    PORT2_IOCR0 &= ~((0x1FU) << SW1_PC0);
    PORT2_IOCR0 |=  ((0x02U) << SW1_PC0);
}

void init_ERU_switch1(void)
{
    /*
     * Slide 117: Switch 1 D2 -> P02.0.
     * Slide 118: P02.0 has SCU input REQ6.
     * Slide 131 ERU input map: REQ6(P02.0) is ERS1 input In10.
     * Therefore Input Channel 1 is used, and EXIS1 must be 000B (input 0),
     * NOT 001B.
     */

    /* Select ERS1 input 0 = REQ6 (P02.0): EXIS1 = 000B. */
    SCU_EICR0 &= ~(0x7U << EXIS1);

    /* Pull-up switch: press causes High -> Low, detect falling edge. */
    SCU_EICR0 |=  (0x1U << FEN1);

    /* Enable trigger event generation for ETL1. */
    SCU_EICR0 |=  (0x1U << EIEN1);

    /* Route ETL1 trigger to OGU0: INP1 = 000B. */
    SCU_EICR0 &= ~(0x7U << INP1);

    /* OGU0: IOUT0 active on trigger event (IGP0 = 01B). */
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

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_blue_LED();
    init_switch1();
    init_ERU_switch1();

    while (1)
    {
        /* Blue LED changes only when the Switch 1 interrupt ISR runs. */
    }
}
