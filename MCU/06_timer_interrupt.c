#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#include "Ifx_Types.h"
#include "IfxStm_reg.h"
#include "IfxSrc_reg.h"
#include "IfxCpu_Irq.h"

#define PORT10_BASE_ADDRESS   (0xF003B000)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))
#define PORT10_OMR            (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x4))

#define PC1                   11
#define PS1                   1
#define PCL1                  17

#define STM0_BASE_ADDRESS     (0xF0000000)
#define STM0_TIM0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10))
#define STM0_CMP0             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x30))
#define STM0_CMCON            (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x38))
#define STM0_ICR              (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x3C))
#define STM0_ISCR             (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x40))

#define CMP0EN                (0)
#define CMP0OS                (2)
#define CMP0IRR               (0)
#define MSIZE0                (0)
#define MSTART0               (8)

#define SRC_BASE_ADDRESS      (0xF0038000)
#define SRC_STM0_SR0          (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0x490))

#define SRE                   (10)
#define TOS                   (11)
#define SRPN                  (0)

#define STM0_FREQUENCY_HZ     (100000000UL)
#define STM0_TICKS_1SEC       (STM0_FREQUENCY_HZ)
#define STM0_ISR_PRIORITY     (10)

volatile unsigned int g_stm0_sec_count = 0;

__interrupt(STM0_ISR_PRIORITY) __vector_table(0)
void Stm0_Compare0_Isr(void)
{
    STM0_ISCR |= ((0x1) << CMP0IRR);
    STM0_CMP0 += (unsigned int)STM0_TICKS_1SEC;
    g_stm0_sec_count++;
    PORT10_OMR |= ((0x1 << PCL1) | (0x1 << PS1));
}

void Init_Timer_1sec_Interrupt(void)
{
    unsigned int currentTime;

    STM0_ICR &= ~((0x1) << CMP0EN);

    SRC_STM0_SR0 &= ~((0x1) << SRE);

    STM0_CMCON &= ~((0x1) << MSTART0);
    STM0_CMCON |= ((0x1F) << MSIZE0);

    STM0_ICR &= ~((0x1) << CMP0OS);

    STM0_ISCR |= ((0x1) << CMP0IRR);

    currentTime = STM0_TIM0;

    STM0_CMP0 = currentTime + (unsigned int)STM0_TICKS_1SEC;

    SRC_STM0_SR0 &= ~(STM0_ISR_PRIORITY << SRPN);
    SRC_STM0_SR0 |= (STM0_ISR_PRIORITY << SRPN);

    SRC_STM0_SR0 &= ~(0x3 << TOS);
    SRC_STM0_SR0 |= ((0x1) << SRE);

    STM0_ICR |= ((0x1) << CMP0EN);
}

void init_LED(void)
{
    // Reset PC1 in Port 10 IOCR0 register
    PORT10_IOCR0 &= ~((0x1F) << PC1);

    // Set PC1 to push-pull mode in Port 10 IOCR0 register
    PORT10_IOCR0 |= ((0x10) << PC1);
}

IfxCpu_syncEvent cpuSyncEvent = 0;

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    Init_Timer_1sec_Interrupt();

    init_LED();

    while (1)
    {
    }
}
