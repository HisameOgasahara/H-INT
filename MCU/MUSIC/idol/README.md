# YOASOBI - Idol - TC275 / Easy Module Shield V1

User-provided `yoasobi-idol-25025-nonstop2k.com.mid`를 TC275 + Easy Module Shield V1의 단일 부저로 재생하는 프로젝트.

## Files

- `Cpu0_Main.c` - AURIX startup / watchdog / CPU sync 후 `idol_run()` 호출.
- `10_idol_player.c` - SW1 PLAY, SW2 ERU interrupt STOP, GTM TOM0_CH11 PWM buzzer, A0 volume control.
- `idol_score.h` - 악보 이벤트 형식, BPM/PPQ, 외부 심볼.
- `idol_score.c` - 사용자 MIDI에서 멜로디 중심으로 추출한 전체 이벤트.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11
- Rotation A0: Easy Module Shield A0 -> ShieldBuddy ADCL.1 -> TC275 SAR4.7 / P32.3 -> VADC Group 4 Channel 7

## Melody selection

잔향산가는 독립된 단선율 Track 0을 그대로 사용했지만, 이번 MIDI는 format 0의 한 트랙 안에 멜로디·화음·베이스가 함께 들어 있다.

- MIDI notes: 1152
- onset groups: 358
- simultaneous/chord onsets: 298
- pitch range: MIDI 27..84

단일 부저에서 저음 반주를 대표음으로 고르지 않도록 다음 방식으로 멜로디를 뽑았다.

1. 같은 absolute tick에서 시작하는 음을 한 onset group으로 묶는다.
2. group의 최고음이 MIDI 56(G#3)보다 낮으면 bass/accompaniment-only onset으로 보고 재생하지 않는다.
3. 나머지 group에서는 가장 높은 음을 melody representative로 선택한다.
4. 같은 최고음이 짧은 음과 지속 화음으로 중복되면 짧은 쪽을 선택해 melody attack이 묻히지 않게 한다.
5. 선택된 음이 다음 melody onset과 겹치면 다음 onset에서 잘라 단선율로 만들고, 비는 구간은 rest로 넣는다.

결과:

- selected melody notes: 327
- generated score events: 407 (`327 notes + 80 rests`)
- selected pitch range: MIDI 56..84
- total: 27584 ticks, 약 77.89초
- tempo: 약 166 BPM
- PPQ: 128 ticks / quarter note

각 이벤트는 다음 형식이다.

```c
{ frequency_hz, midi_ticks }
```

`frequency_hz == 0`은 rest다.

## Rotation A0 volume control

잔향산가/포켓몬/코난 예제와 같은 VADC 경로를 사용한다.

1. A0를 `VADC Group 4 / Channel 7`에서 12-bit 값 `0..4095`로 읽는다.
2. 현재 음의 TOM0_CH11 period는 유지한다.
3. ADC 값에 따라 duty만 `0..50%`로 바꿔 음정은 유지한 채 체감 음량을 조절한다.
4. 음 재생 대기 루프에서 약 5 ms마다 A0를 다시 읽으므로 재생 중에도 바로 반영된다.

A0 최소 쪽은 거의 무음, 최대 쪽은 50% duty다.

## Controls

1. SW1을 누르면 처음부터 재생한다.
2. 재생 중 SW2를 누르면 ERU interrupt로 즉시 정지한다.
3. 재생 중 Rotation A0를 돌리면 음량이 바뀐다.
4. 정지 후 SW1을 놓았다 다시 누르면 처음부터 재생한다.

## Build

한 AURIX 프로젝트에 아래 네 파일을 넣는다.

- `Cpu0_Main.c`
- `10_idol_player.c`
- `idol_score.c`
- `idol_score.h`

다른 MUSIC 예제의 player는 같은 ERU/PWM 자원을 사용하므로 동시에 넣지 않는다.
