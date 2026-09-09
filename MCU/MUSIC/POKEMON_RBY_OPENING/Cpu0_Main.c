#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * Cpu0 entry point for the Pokemon Red/Blue/Yellow opening project.
 *
 * The playback module owns the application loop, so core0_main() only
 * performs the normal AURIX startup sequence and then enters the player.
 */
extern void pokemon_rby_opening_run(void);

IfxCpu_syncEvent cpuSyncEvent = 0;

int core0_main(void)
{
    IfxCpu_enableInterrupts();

    /*
     * Same startup pattern used by the MCU course examples.
     * The player accesses protected CLC registers during peripheral init,
     * while its own ENDINIT sequence handles those writes.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Synchronize CPU cores before starting the application. */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    /*
     * Does not return during normal operation.
     * SW1 = PLAY, SW2 = interrupt STOP, Rotation A0 = volume.
     */
    pokemon_rby_opening_run();

    while (1)
    {
    }

    return (1);
}
