#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT2_BASE_ADDRESS    (0xF003A200)
#define PORT10_BASE_ADDRESS   (0xF003B000)

#define PORT2_IOCR0           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10))

#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

/* Switch 1: Shield D2 -> TC275 P02.0 */
#define SW1_PC0               3

/* Blue LED: Shield D13 -> TC275 P10.2 */
#define BLUE_PC2              19
#define BLUE_PS2              2
#define BLUE_PCL2             18

/* SCU ERU */
#define SCU_BASE_ADDRESS      (0xF0036000)
#define SCU_EICR0             (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x210))
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
 * Interrupt control practice I (slide 160)
 * Switch 1 press toggles the blue LED.
 */
__interrupt(0x0F) __vector_table(0)
void ISR0(void)
{
    /* P10.2 toggle: PCL2=1 and PS2=1 in OMR */
    PORT10_OMR = ((0x1U << BLUE_PCL2) | (0x1U << BLUE_PS2));
}

void init_blue_LED(void)
{
    /* P10.2 -> push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1FU) << BLUE_PC2);
    PORT10_IOCR0 |=  ((0x10U) << BLUE_PC2);

    /* Start with blue LED OFF */
    PORT10_OUTPUT &= ~(0x1U << BLUE_PS2);
}

void init_switch1(void)
{
    /* P02.0 -> general-purpose input with pull-up */
    PORT2_IOCR0 &= ~((0x1FU) << SW1_PC0);
    PORT2_IOCR0 |=  ((0x02U) << SW1_PC0);
}

void init_ERU_switch1(void)
{
    /*
     * Switch 1 is D2 -> P02.0 -> REQ6.
     * REQ6 is selected through ERS0 input 1, therefore EXIS0 = 001B.
     */
    SCU_EICR0 &= ~(0x7U << EXIS0);
    SCU_EICR0 |=  (0x1U << EXIS0);

    /* Pull-up switch: pressed means High -> Low, so detect falling edge. */
    SCU_EICR0 |=  (0x1U << FEN0);

    /* Enable trigger event generation. */
    SCU_EICR0 |=  (0x1U << EIEN0);

    /* Route ETL0 trigger to OGU0. */
    SCU_EICR0 &= ~(0x7U << INP0);

    /* OGU0: always enable IOUT0 on trigger. */
    SCU_IGCR0 &= ~(0x3U << IGP0);
    SCU_IGCR0 |=  (0x1U << IGP0);

    /* Interrupt priority = 0x0F. */
    SRC_SCU_ERU0 &= ~(0xFFU);
    SRC_SCU_ERU0 |=  (0x0FU);

    /* Enable service request. */
    SRC_SCU_ERU0 |=  (0x1U << SRE);

    /* Route to CPU0. */
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
        /* LED changes only when the interrupt ISR runs. */
    }
}
