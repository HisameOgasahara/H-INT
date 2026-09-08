#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxStm_reg.h"
#include "IfxSrc_reg.h"
#include "IfxCpu_Irq.h"

/* P10.1 red LED */
#define PORT10_BASE_ADDRESS   (0xF003B000U)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10U))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x00U))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04U))

#define PC1                   (11U)
#define PS1                   (1U)
#define PCL1                  (17U)

/* STM0 registers */
#define STM0_BASE_ADDRESS     (0xF0000000U)
#define STM0_TIM0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))
#define STM0_CMP0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x30U))
#define STM0_CMCON            (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x38U))
#define STM0_ICR              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x3CU))
#define STM0_ISCR             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x40U))

/* STM0 ICR / ISCR / CMCON bit positions */
#define CMP0EN                (0U)
#define CMP0OS                (2U)
#define CMP0IRR               (0U)
#define MSIZE0                (0U)
#define MSTART0               (8U)

/* Interrupt Router: STM0 service request 0 */
#define SRC_BASE_ADDRESS      (0xF0038000U)
#define SRC_STM0_SR0          (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0x490U))

#define SRE                   (10U)
#define TOS                   (11U)
#define SRPN                  (0U)

/* Slide 190~193: STM0 runs at 100 MHz, so 100,000,000 ticks = 1 second. */
#define STM0_FREQUENCY_HZ     (100000000UL)
#define STM0_TICKS_1SEC       (STM0_FREQUENCY_HZ)
#define STM0_ISR_PRIORITY     (10U)

volatile unsigned int g_stm0_sec_count = 0U;

IfxCpu_syncEvent cpuSyncEvent = 0;

/*
 * STM0 Comparator 0 interrupt handler.
 * 1) Clear CMP0 interrupt request flag.
 * 2) Reserve the next interrupt from the previous compare value, not from
 *    the current timer value, so ISR entry latency does not accumulate.
 * 3) Toggle P10.1 red LED.
 */
__interrupt(STM0_ISR_PRIORITY) __vector_table(0)
void Stm0_Compare0_Isr(void)
{
    STM0_ISCR |= (0x1U << CMP0IRR);
    STM0_CMP0 += (unsigned int)STM0_TICKS_1SEC;

    g_stm0_sec_count++;

    /* Toggle P10.1: PS1 sets it, PCL1 clears it. */
    PORT10_OMR |= ((0x1U << PCL1) | (0x1U << PS1));
}

/*
 * Configure STM0 Comparator 0 for a 1-second periodic interrupt.
 * This implements the sequence described on slides 187~189.
 */
void Init_Timer_1sec_Interrupt(void)
{
    unsigned int currentTime;

    /* 1. Disable STM0 Comparator 0 interrupt while configuring. */
    STM0_ICR &= ~(0x1U << CMP0EN);

    /* 2. Disable STM0SR0 service request while configuring. */
    SRC_STM0_SR0 &= ~(0x1U << SRE);

    /* 3. Compare TIM0[31:0] with CMP0[31:0].
     *    MSTART0 = 0, MSIZE0 = 31.
     */
    STM0_CMCON &= ~(0x1FU << MSTART0);
    STM0_CMCON &= ~(0x1FU << MSIZE0);
    STM0_CMCON |=  (0x1FU << MSIZE0);

    /* 4. Route Comparator 0 match event to STM0SR0 (CMP0OS = 0). */
    STM0_ICR &= ~(0x1U << CMP0OS);

    /* 5. Clear any stale Comparator 0 interrupt request flag. */
    STM0_ISCR |= (0x1U << CMP0IRR);

    /* 6. Read current lower 32-bit STM0 timer value. */
    currentTime = STM0_TIM0;

    /* 7. First compare match occurs one second from now. */
    STM0_CMP0 = currentTime + (unsigned int)STM0_TICKS_1SEC;

    /* 8. Interrupt Router: priority 10, CPU0, service request enabled. */
    SRC_STM0_SR0 &= ~(0xFFU << SRPN);
    SRC_STM0_SR0 |=  (STM0_ISR_PRIORITY << SRPN);

    SRC_STM0_SR0 &= ~(0x3U << TOS);   /* TOS = 0 -> CPU0 */
    SRC_STM0_SR0 |=  (0x1U << SRE);

    /* 9. Enable STM0 Comparator 0 interrupt. */
    STM0_ICR |= (0x1U << CMP0EN);
}

void init_LED(void)
{
    /* Reset PC1 field in P10_IOCR0. */
    PORT10_IOCR0 &= ~(0x1FU << PC1);

    /* Set P10.1 as push-pull general-purpose output. */
    PORT10_IOCR0 |= (0x10U << PC1);

    /* Start with LED off. */
    PORT10_OUTPUT &= ~(0x1U << PS1);
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if required.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    Init_Timer_1sec_Interrupt();
    init_LED();

    while (1)
    {
        /* Main loop stays idle; LED toggling is performed by the STM ISR. */
    }
}
