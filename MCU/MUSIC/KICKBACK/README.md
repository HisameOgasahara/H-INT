# KICK BACK - TC275 / Easy Module Shield V1

## Files

- `10_kickback_player.c` - TC275 playback engine. Handles SW1 PLAY, SW2 ERU interrupt STOP, STM timing, and GTM TOM0_CH11 PWM buzzer output.
- `kickback_score.h` - score event type, constants, and `extern` declarations shared by the player and score data.
- `kickback_score.c` - actual monophonic KICK BACK score event table generated from the provided MIDI.

`kickback_player.h` is no longer used. The player implementation exports `kickback_run()` directly.

## Integration with AURIX Development Studio

`10_kickback_player.c` intentionally does not define `core0_main()` or `cpuSyncEvent`, so it does not collide with the normal generated `Cpu0_Main.c`.

In `Cpu0_Main.c`, declare the player entry point and call it after the normal startup/watchdog/core-sync sequence:

```c
extern void kickback_run(void);
```

Then call:

```c
kickback_run();
```

`kickback_run()` owns the application loop after it is entered.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11

## Score model

The source music contains simultaneous notes/chords, while one buzzer/PWM channel can generate only one fundamental frequency at a time. The current score therefore uses the requested monophonic representative/root-like reduction.

The provided MIDI has no Set Tempo event, so the player supplies quarter-note = 204 BPM from the score/PDF reference.

Each score event is:

```c
{ frequency_hz, duration_in_16th_note_ticks }
```

`frequency_hz == 0` means rest. At 204 BPM one 1/16-note tick is about 73.53 ms.

## Controls

1. Press SW1 once to start playback.
2. Press SW2 during playback to trigger the ERU interrupt and stop immediately.
3. Release SW1 and press it again to restart from the beginning.

## Current score generation

`kickback_score.c` is generated from the provided MIDI rather than manually transcribed from the rendered PDF. Simultaneous note onsets are reduced to one representative low/root-like pitch so the score can be played by the single buzzer channel. The playback engine is independent of the score table, so the score data can be replaced later without changing the port, interrupt, STM, or PWM driver logic.
