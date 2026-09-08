#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxStm_reg.h"
#include "IfxSrc_reg.h"
#include "IfxCpu_Irq.h"

/*
 * MCU programming slide 194 exercise
 *
 * Every 0.5 s, D12(red) / D13(blue) LEDs follow this sequence:
 *   0: both OFF
 *   1: red ON,  blue OFF
 *   2: red OFF, blue ON
 *   3: red ON,  blue ON
 *   4: red OFF, blue ON
 *   5: red ON,  blue OFF
 * then repeat.
 *
 * ShieldBuddy TC275 mapping:
 *   Arduino D12 -> TC275 P10.1 -> Easy Module Shield red LED
 *   Arduino D13 -> TC275 P10.2 -> Easy Module Shield blue LED
 */

#define PORT10_BASE_ADDRESS   (0xF003B000U)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10U))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04U))

/* P10.1 : red LED */
#define PC1                   (11U)
#define PS1                   (1U)
#define PCL1                  (17U)

/* P10.2 : blue LED */
#define PC2                   (19U)
#define PS2                   (2U)
#define PCL2                  (18U)

/* STM0: CPU0 system timer */
#define STM0_BASE_ADDRESS     (0xF0000000U)
#define STM0_TIM0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))
#define STM0_CMP0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x30U))
#define STM0_CMCON            (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x38U))
#define STM0_ICR              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x3CU))
#define STM0_ISCR             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x40U))

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

/* ShieldBuddy STM timing base: 10 ns/tick = 100 MHz. */
#define STM0_FREQUENCY_HZ     (100000000UL)
#define STM0_TICKS_500MS      (STM0_FREQUENCY_HZ / 2UL)
#define STM0_ISR_PRIORITY     (10U)

IfxCpu_syncEvent cpuSyncEvent = 0;

/*
 * State 0 (both OFF) is applied at startup.
 * The first interrupt, 0.5 s later, advances to state 1.
 */
volatile unsigned int g_led_state = 1U;

static void set_led_state(unsigned int state)
{
    switch (state)
    {
        case 0U: /* both OFF */
            PORT10_OMR = (0x1U << PCL1) | (0x1U << PCL2);
            break;

        case 1U: /* red ON, blue OFF */
            PORT10_OMR = (0x1U << PS1) | (0x1U << PCL2);
            break;

        case 2U: /* red OFF, blue ON */
            PORT10_OMR = (0x1U << PCL1) | (0x1U << PS2);
            break;

        case 3U: /* red ON, blue ON */
            PORT10_OMR = (0x1U << PS1) | (0x1U << PS2);
            break;

        case 4U: /* red OFF, blue ON */
            PORT10_OMR = (0x1U << PCL1) | (0x1U << PS2);
            break;

        case 5U: /* red ON, blue OFF */
            PORT10_OMR = (0x1U << PS1) | (0x1U << PCL2);
            break;

        default:
            PORT10_OMR = (0x1U << PCL1) | (0x1U << PCL2);
            break;
    }
}

__interrupt(STM0_ISR_PRIORITY) __vector_table(0)
void Stm0_Compare0_Isr(void)
{
    /* Clear Comparator 0 interrupt request flag. */
    STM0_ISCR |= (0x1U << CMP0IRR);

    /* Keep an exact 0.5 s period without accumulating ISR entry latency. */
    STM0_CMP0 += (unsigned int)STM0_TICKS_500MS;

    set_led_state(g_led_state);

    g_led_state++;
    if (g_led_state >= 6U)
    {
        g_led_state = 0U;
    }
}

void Init_Timer_500ms_Interrupt(void)
{
    unsigned int currentTime;

    /* 1. Disable Comparator 0 interrupt during configuration. */
    STM0_ICR &= ~(0x1U << CMP0EN);

    /* 2. Disable STM0SR0 service request during configuration. */
    SRC_STM0_SR0 &= ~(0x1U << SRE);

    /* 3. Compare STM0_TIM0[31:0] with CMP0[31:0]. */
    STM0_CMCON &= ~(0x1FU << MSTART0);
    STM0_CMCON &= ~(0x1FU << MSIZE0);
    STM0_CMCON |=  (0x1FU << MSIZE0);

    /* 4. Comparator 0 match -> STM0SR0. */
    STM0_ICR &= ~(0x1U << CMP0OS);

    /* 5. Clear a stale Comparator 0 request flag. */
    STM0_ISCR |= (0x1U << CMP0IRR);

    /* 6~7. First compare match: 0.5 s from now. */
    currentTime = STM0_TIM0;
    STM0_CMP0 = currentTime + (unsigned int)STM0_TICKS_500MS;

    /* 8. Priority 10, CPU0, service request enabled. */
    SRC_STM0_SR0 &= ~(0xFFU << SRPN);
    SRC_STM0_SR0 |=  (STM0_ISR_PRIORITY << SRPN);
    SRC_STM0_SR0 &= ~(0x3U << TOS);
    SRC_STM0_SR0 |=  (0x1U << SRE);

    /* 9. Enable Comparator 0 interrupt. */
    STM0_ICR |= (0x1U << CMP0EN);
}

void init_LED(void)
{
    /* P10.1 -> push-pull general-purpose output (PC1 = 10000B). */
    PORT10_IOCR0 &= ~(0x1FU << PC1);
    PORT10_IOCR0 |=  (0x10U << PC1);

    /* P10.2 -> push-pull general-purpose output (PC2 = 10000B). */
    PORT10_IOCR0 &= ~(0x1FU << PC2);
    PORT10_IOCR0 |=  (0x10U << PC2);

    /* Initial state from slide 194: both LEDs OFF. */
    set_led_state(0U);
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!! */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();
    Init_Timer_500ms_Interrupt();

    while (1)
    {
        /* LED sequence is driven only by the STM0 interrupt. */
    }
}
