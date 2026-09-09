# Pokemon Red/Blue/Yellow Opening - TC275 / Easy Module Shield V1

KICKBACK 프로젝트의 검증된 TC275 재생 구조를 그대로 응용한 단음 부저 버전이다.

## Files

- `Cpu0_Main.c` - AURIX CPU0 진입점. watchdog 비활성화, CPU sync 후 player를 호출한다.
- `10_pokemon_rby_opening.c` - SW1 재생, SW2 ERU 인터럽트 정지, GTM TOM0_CH11 PWM 출력, RTTTL 파서, A0 볼륨 제어.
- `pokemon_rby_score.h` - 전체 RTTTL 데이터 인터페이스와 192 BPM 설정.
- `pokemon_rby_full_score.c` - Pokemon Red/Blue/Yellow opening의 전체 단음 RTTTL 데이터.

네 파일 모두 실제 빌드에 사용된다. 별도의 player header는 두지 않았다.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11
- Rotation A0 potentiometer: Easy Module Shield A0 -> ShieldBuddy ADCL.1 -> TC275 SAR4.7 / P32.3 -> VADC Group 4 Channel 7

YwRobot Easy Module Shield V1에서 D2=SW1, D3=SW2, D5=buzzer, A0=rotation potentiometer이며, TC275 쪽 포트/ERU/GTM 경로는 기존 KICKBACK 프로젝트에서 동작 확인한 설정을 재사용한다.

## How it works

KICKBACK처럼 `{Hz, tick}` 배열을 미리 펼치는 대신, 이 프로젝트는 RTTTL 문자열을 MCU에서 직접 읽는다.

각 토큰의 음표명/옥타브를 정수 Hz로 변환하고, `GTM TOM0_CH11`의 PWM period를 바꾸어 음높이를 만든다. 음 길이는 RTTTL의 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 등의 denominator와 dotted-note 여부를 읽어 STM0으로 기다린다.

Tempo는 원본 transcription의 192 BPM을 사용한다.

### Rotation A0 volume control

강의자료의 potentiometer ADC 실습과 같은 경로를 사용한다.

1. A0의 아날로그 전압을 `VADC Group 4 / Channel 7`에서 12-bit 값(0~4095)으로 읽는다.
2. 음높이를 결정하는 TOM0_CH11 period는 그대로 둔다.
3. A0 값에 따라 TOM0_CH11 duty를 `0% ~ 50%` 범위로 바꿔 체감 음량을 조절한다.
4. 재생 중 약 5 ms마다 A0를 다시 읽으므로 노브를 돌리면 현재 재생 중인 음에도 바로 반영된다.

A0 최소 쪽은 거의 무음, 최대 쪽은 기존 50% duty에 해당한다. Active/passive buzzer의 실제 음량 반응은 선형적이지 않을 수 있다.

## Controls

1. SW1을 한 번 누르면 처음부터 재생한다.
2. 재생 중 SW2를 누르면 ERU interrupt가 발생하여 즉시 PWM duty를 0으로 만들고 정지한다.
3. 재생 중 Rotation A0를 돌리면 부저의 체감 음량이 바뀐다.
4. SW1을 놓았다가 다시 누르면 처음부터 다시 재생한다.

## AURIX project integration

`Cpu0_Main.c`가 `core0_main()`과 `cpuSyncEvent`를 유일하게 정의한다. startup/watchdog/sync 뒤에서 player 진입 함수만 호출한다.

```c
extern void pokemon_rby_opening_run(void);

IfxCpu_syncEvent cpuSyncEvent = 0;

int core0_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    pokemon_rby_opening_run();

    while (1) { }
    return (1);
}
```

`10_pokemon_rby_opening.c`에는 `core0_main()`이나 `cpuSyncEvent`를 정의하지 않는다. 따라서 AURIX Development Studio 프로젝트에서는 이 폴더의 `Cpu0_Main.c`를 CPU0 main 파일로 사용하고, 나머지 세 파일을 함께 빌드하면 된다.

`KICKBACK/10_kickback_player.c`와 이 파일은 둘 다 같은 SW2 ERU0/priority와 같은 buzzer TOM 채널을 사용하는 대체 예제다. 하나의 AURIX 실행 프로젝트에 두 음악 player를 동시에 넣지 말고, 재생할 프로젝트 하나만 선택한다.

## Score source

Monophonic transcription basis: RTTTL.com, `Pokemon Opening (Red, Blue, Yellow)`, 192 BPM, 775 notes, about 85.2 seconds. 현재 코드는 발췌본이 아니라 이 전체 transcription을 사용한다.
