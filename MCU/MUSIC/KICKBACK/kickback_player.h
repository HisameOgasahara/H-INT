#ifndef KICKBACK_PLAYER_H
#define KICKBACK_PLAYER_H

/*
 * Entry point for the KICK BACK application module.
 *
 * Cpu0_Main.c remains the only file that defines core0_main() and cpuSyncEvent.
 * After the normal AURIX startup/watchdog/sync sequence, call kickback_run().
 */
void kickback_run(void);

#endif /* KICKBACK_PLAYER_H */
