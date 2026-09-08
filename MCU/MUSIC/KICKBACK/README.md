# KICK BACK - TC275 / Easy Module Shield V1

## Files

- `10_kickback_player.c` - TC275 playback engine, SW1 PLAY, SW2 ERU interrupt STOP, GTM TOM0_CH11 PWM buzzer output.
- `kickback_score.h` - monophonic representative-tone score data.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11

## Score model

The provided score PDF is in 4/4 at quarter-note = 204 BPM and contains many simultaneous notes/chords. One buzzer/PWM channel can generate only one fundamental frequency at a time, so this first version uses the requested **representative/root-like note reduction** rather than attempting polyphony.

Each score event is:

```c
{ frequency_hz, duration_in_16th_note_ticks }
```

`frequency_hz == 0` means rest. At 204 BPM one 1/16-note tick is about 73.53 ms.

## Controls

1. Press SW1 once to start playback.
2. Press SW2 during playback to trigger the ERU interrupt and stop immediately.
3. Release SW1 and press it again to restart from the beginning.

## Important limitation of this version

The uploaded PDF is a rendered notation page rather than machine-readable MIDI/MusicXML. Therefore `kickback_score.h` is a **first-pass representative-tone arrangement**, not a guaranteed note-for-note transcription of all 159 measures. The playback engine is already independent of the score, so replacing the table with a MIDI-derived table later does not require changing the MCU driver/interrupt/PWM code.
