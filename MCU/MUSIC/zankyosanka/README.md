# Zankyo Zanka - TC275 / Easy Module Shield V1

User-provided `Demon Slayer OP 3 ~ Zankyō Zanka.mid`를 TC275 + Easy Module Shield V1의 단일 부저로 재생하는 프로젝트.

## Files

- `Cpu0_Main.c` - AURIX startup / watchdog / CPU sync 후 `zankyosanka_run()` 호출.
- `10_zankyosanka_player.c` - SW1 PLAY, SW2 ERU interrupt STOP, GTM TOM0_CH11 PWM buzzer player.
- `zankyosanka_score.h` - 악보 이벤트 형식, BPM/PPQ, 외부 심볼.
- `zankyosanka_score.c` - 사용자 MIDI에서 추출한 전체 멜로디 이벤트.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11

## Melody selection

KICKBACK 프로젝트는 화음 onset에서 가장 낮은 새 음을 대표음으로 선택했다.

이번 MIDI는 구조가 더 명확하다.

- MIDI Track 0: 262 notes, MIDI note 71..99, simultaneous onset 0회 -> 이미 단선율 melody track.
- MIDI Track 1: 454 notes, MIDI note 49..75, 다수의 2음 동시 onset -> accompaniment/chord track.

따라서 이번에는 화음을 임의 축약하지 않고 **Track 0을 그대로 melody로 사용**한다. 첨부 PDF의 위쪽 보표에 나타나는 고음 멜로디 라인과도 구조가 대응한다.

## Timing

- MIDI tempo: 160 BPM (`set_tempo = 375000 us/quarter`)
- PPQ: 128 ticks / quarter note
- melody notes: 262
- rest gaps included
- generated player events: 523
- total duration: 32240 MIDI ticks = about 94.453 s

각 이벤트:

```c
{ frequency_hz, midi_ticks }
```

`frequency_hz == 0`은 rest.

## Controls

1. SW1을 누르면 처음부터 재생.
2. 재생 중 SW2를 누르면 ERU interrupt로 즉시 정지.
3. SW1을 놓았다 다시 누르면 처음부터 재생.

## Build

한 AURIX 프로젝트에 아래 파일을 넣는다.

- `Cpu0_Main.c`
- `10_zankyosanka_player.c`
- `zankyosanka_score.c`
- `zankyosanka_score.h`

다른 MUSIC 예제의 player는 같은 ERU/PWM 자원을 사용하므로 동시에 넣지 않는다.
