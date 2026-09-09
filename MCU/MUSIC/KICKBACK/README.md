# KICK BACK - TC275 / Easy Module Shield V1

사용자 제공 `Chainsaw Man OP ~ Kick Back.mid`를 TC275 + Easy Module Shield V1의 단일 부저로 재생하는 프로젝트.

## Files

- `10_kickback_player.c` - SW1 PLAY, SW2 ERU interrupt STOP, GTM TOM0_CH11 PWM buzzer, A0 volume control.
- `kickback_score.h` - MIDI tick 기반 score event 형식, BPM/PPQ, 외부 심볼.
- `kickback_score.c` - 사용자 MIDI에서 멜로디 중심으로 다시 추출한 KICK BACK 전체 이벤트.

`10_kickback_player.c`는 `core0_main()`과 `cpuSyncEvent`를 정의하지 않는다. 기존 AURIX 프로젝트의 `Cpu0_Main.c`에서 `extern void kickback_run(void);`를 선언하고 startup/watchdog/core-sync 뒤에 `kickback_run();`을 호출한다.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11
- Rotation A0: Easy Module Shield A0 -> ShieldBuddy ADCL.1 -> TC275 SAR4.7 / P32.3 -> VADC Group 4 Channel 7

## Melody selection

기존 버전은 화음 onset에서 **가장 낮은 새 음**을 대표음으로 골라 root/bass 쪽으로 축약했다. 이번 버전은 잔향산가 이후의 원칙에 맞춰 **멜로디 라인을 우선**하도록 완전히 다시 생성했다.

이번 MIDI는 format 1, PPQ 128이며 두 트랙의 역할이 분리되어 있다.

- Track 0: 297 notes, MIDI 63..81 -> 악보의 위쪽 보표/멜로디 계열
- Track 1: 497 notes, MIDI 25..56 -> 아래쪽 보표/반주·베이스 계열

따라서 다음 방식으로 단일 부저용 멜로디를 만든다.

1. **Track 0만** 멜로디 후보로 사용한다.
2. 같은 absolute tick에서 시작하는 음을 하나의 onset group으로 묶는다.
3. Track 0의 252 onset group 중 44개는 2~3음이 동시에 시작하므로, 이 경우 **가장 높은 새 음**을 대표 멜로디로 선택한다.
4. 선택된 음이 다음 melody onset과 겹치면 다음 onset 시점에서 잘라 한 번에 한 음만 울리게 한다.
5. 음이 끝난 뒤 다음 onset까지 비는 구간은 explicit rest로 넣는다.

첨부 PDF에서도 위쪽 보표가 보컬/주선율을 담당하고 아래쪽 보표가 저음 반주를 담당하는 구조라 이 선택과 맞는다.

결과:

- selected melody notes: 252
- generated events: 503
- selected pitch range: MIDI 63..81
- tempo: 150 BPM (`400000 us/quarter`)
- PPQ: 128 ticks / quarter note
- total: 38400 MIDI ticks = 120.0 s

각 이벤트:

```c
{ frequency_hz, midi_ticks }
```

`frequency_hz == 0`은 rest다.

## Rotation A0 volume control

잔향산가/IDOL/포켓몬/코난 예제와 같은 VADC 방식이다.

1. A0를 `VADC Group 4 / Channel 7`에서 12-bit `0..4095`로 읽는다.
2. TOM0_CH11의 period는 현재 음정에 맞게 유지한다.
3. A0 값에 따라 duty만 `0..50%`로 바꿔 음정은 유지하고 체감 음량만 조절한다.
4. 재생 대기 루프에서 약 5 ms마다 A0를 다시 읽으므로 재생 중에도 바로 반영된다.

## Controls

1. SW1을 누르면 처음부터 재생.
2. 재생 중 SW2를 누르면 ERU interrupt로 즉시 정지.
3. 재생 중 Rotation A0를 돌리면 음량 조절.
4. SW1을 놓았다 다시 누르면 처음부터 재생.

## Build

AURIX 프로젝트에 다음 파일을 넣는다.

- `10_kickback_player.c`
- `kickback_score.c`
- `kickback_score.h`

그리고 기존 `Cpu0_Main.c`에서 `kickback_run()`을 호출한다. 다른 MUSIC player는 같은 ERU/PWM 자원을 사용하므로 동시에 포함하지 않는다.
