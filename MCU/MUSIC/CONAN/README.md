# Detective Conan Main Theme - TC275 / Easy Module Shield V1

기존 KICKBACK / POKEMON 예제와 같은 TC275 + Easy Module Shield V1 구조를 사용한 단음 부저 연주 프로젝트다.

## Files

- `Cpu0_Main.c` - AURIX startup / watchdog / CPU sync 뒤 `conan_main_theme_run()` 호출.
- `10_conan_main_theme.c` - SW1 재생, SW2 ERU interrupt 정지, GTM TOM0_CH11 PWM 출력, A0 실시간 볼륨 제어.
- `conan_score.h` - 단음 이벤트 구조와 145 BPM 설정.
- `conan_score.c` - Detective Conan Main Theme 멜로디 이벤트 데이터.

## Hardware mapping

- SW1: Easy Module Shield D2 -> ShieldBuddy TC275 P02.0
- SW2: Easy Module Shield D3 -> ShieldBuddy TC275 P02.1
- Buzzer: Easy Module Shield D5 -> ShieldBuddy TC275 P02.3 -> GTM TOUT3 -> TOM0_CH11
- Rotation A0: Easy Module Shield A0 -> TC275 SAR4.7 / P32.3 -> VADC Group 4 Channel 7

## Controls

1. SW1을 누르면 처음부터 재생한다.
2. 재생 중 SW2를 누르면 ERU interrupt로 즉시 정지한다.
3. 재생 중 Rotation A0를 돌리면 PWM duty가 바뀌어 체감 음량이 변한다.
4. SW1을 놓았다가 다시 누르면 처음부터 다시 재생한다.

## Score source / adaptation

피치 진행은 공개된 `Detective Conan Main Theme OST` easy kalimba number-note transcription의 전체 표기 구간을 사용했다. 숫자 `1..7`은 C-D-E-F-G-A-B로, `*`는 한 옥타브 위, `**`는 두 옥타브 위로 변환했다.

참고한 공개 자료:
- KalimbaTabs.net `Detective Conan Main Theme OST` - 전체 easy number-note melody.
- Music Box Maniacs `Detective Conan Main Theme` - Grand Illusions 30 arrangement, BPM 145, length 750.

중요: KalimbaTabs 공개 텍스트에는 개별 음 길이가 표기되어 있지 않으므로, 이 MCU 버전은 각 음을 기본 1/8-note pulse로 재생하고 각 원문 행 끝에 짧은 rest를 넣은 **buzzer용 리듬 재구성**이다. 피치 순서는 공개 멜로디를 따르지만 원곡의 정확한 박자/아티큘레이션을 그대로 복원한 MIDI 전사는 아니다.

## Build

AURIX Development Studio 프로젝트에서 아래 네 파일을 소스에 넣으면 된다.

- `Cpu0_Main.c`
- `10_conan_main_theme.c`
- `conan_score.c`
- `conan_score.h`

`KICKBACK`, `POKEMON_RBY_OPENING`, `CONAN` player는 같은 ERU/TOM 자원을 쓰므로 하나의 실행 프로젝트에는 한 player만 선택한다.
