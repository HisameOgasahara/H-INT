#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#define PORT10_BASE_ADDRESS   (0xF003B000)
#define PORT10_IOCR0          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_OUTPUT         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))

#define PC2  19
#define P2   2

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_LED(void)
{
    /* P10.2 설정 비트 초기화 */
    PORT10_IOCR0 &= ~((0x1F) << PC2);

    /* P10.2를 push-pull output으로 설정 */
    PORT10_IOCR0 |= ((0x10) << PC2);
}

void blink_LED(void)
{
    static int LED_flag = 1;

    if (LED_flag == 1)
    {
        PORT10_OUTPUT |= (0x1 << P2);      // P10.2 High
        LED_flag = 0;
    }
    else
    {
        PORT10_OUTPUT &= ~(0x1 << P2);     // P10.2 Low
        LED_flag = 1;
    }
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(
        IfxScuWdt_getCpuWatchdogPassword());

    IfxScuWdt_disableSafetyWatchdog(
        IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_LED();

    while (1)
    {
        blink_LED();

        for (int cycle = 0; cycle < 10000000; cycle++);
    }
}
