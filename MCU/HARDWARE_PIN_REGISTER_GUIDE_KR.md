# TC275 + ShieldBuddy + Easy Module Shield V1 하드웨어 / 핀 / 레지스터 가이드

이 문서는 `MCU/` 실습에서 반복해서 사용하는 **하드웨어 연결 관계와 레지스터 구조를 한 곳에 정리한 한국어 참고 문서**이다.

기준은 다음 자료와 이 저장소에서 실제로 동작 확인한 코드들이다.

- `MCU 프로그래밍(SW)_최규상.pdf`
- `infineon-shieldbuddy-tc275-usermanual-en.pdf`
- `infineon-tc27x-d-step-usermanual-en.pdf`
- `YwRobot Easy Module Shield V1.pdf`
- 저장소의 `01_led.c` ~ `10_scale_interrupt_stop.c`
- 특히 종합 예제인 [`09_vehicle_system.c`](./09_vehicle_system.c), [`09_turn_signals.c`](./09_turn_signals.c)

> 가장 중요한 구분: **Easy Module Shield의 `D12`, `A0` 같은 이름은 실드/Arduino 커넥터 기준 이름이고, 실제 TC275에서 프로그램이 만지는 핀은 `P10.1`, `P32.3` 같은 MCU 포트 핀이다.**

---

## 1. 실습 하드웨어가 어떻게 겹쳐져 있는가

실습 장비는 아래 3계층으로 생각하면 헷갈리지 않는다.

```text
[Easy Module Shield V1]
 LED / SW / RGB LED / Buzzer / Potentiometer / LDR ...
              │ Arduino 호환 D0~D13, A0~A3
              ▼
[Hitex ShieldBuddy TC275 보드]
 Arduino 형식 커넥터를 TC275 핀으로 배선
              │
              ▼
[Infineon AURIX TC275 MCU]
 P02.x / P10.x / P32.x / VADC / STM / GTM / ERU ...
```

즉 코드 작성 시 실제 흐름은 보통 다음과 같다.

```text
실드 부품
→ 실드 핀 이름(D12, A0 ...)
→ ShieldBuddy 배선
→ TC275 물리 핀(P10.1, P32.3 ...)
→ PORT / VADC / GTM / STM / ERU 같은 주변장치
→ 해당 주변장치의 메모리 맵 레지스터
```

---

## 2. 실습에서 가장 많이 쓰는 핀 전체표

### 2.1 Easy Module Shield V1 → TC275

| 실드 부품 | 실드 핀 | ShieldBuddy → TC275 | 실습에서 주로 쓰는 기능 |
|---|---:|---|---|
| Potentiometer(파란 회전 노브) | A0 | `SAR4.7 / P32.3` | VADC Group 4 Channel 7 |
| LDR(조도 센서) | A1 | `SAR4.6 / P32.4` | VADC Group 4 Channel 6 |
| LM35 온도센서 | A2 | `SAR4.5 / P32.1` | VADC Group 4 Channel 5 |
| Analog 확장 | A3 | `SAR4.4 / P23.2` | VADC Group 4 Channel 4 |
| Serial RX | D0 | `P15.3` | ASCLIN0 RX |
| Serial TX | D1 | `P15.2` | ASCLIN0 TX |
| SW1 | D2 | `P02.0` | GPIO 입력 / ERU 외부 인터럽트 |
| SW2 | D3 | `P02.1` | GPIO 입력 |
| DHT11 | D4 | `P10.4` | Digital I/O |
| Active Buzzer | D5 | `P02.3` | GPIO 출력 또는 `GTM TOUT3` PWM |
| IR Receiver | D6 | `P02.4` | Digital input |
| Digital 확장 | D7 | `P02.5` | Digital I/O |
| Digital 확장 | D8 | `P02.6` | Digital I/O |
| RGB LED - Red | D9 | `P02.7` | GPIO/PWM 출력 |
| RGB LED - Green | D10 | `P10.5` | GPIO/PWM 출력 |
| RGB LED - Blue | D11 | `P10.3` | GPIO/PWM 출력 |
| LED2 Red | D12 | `P10.1` | GPIO 또는 `GTM TOUT103` PWM |
| LED1 Blue | D13 | `P10.2` | GPIO 출력 |

Easy Module Shield 자체 회로 기준으로는 다음도 중요하다.

- `SW1`, `SW2`는 **누르면 LOW**가 된다. 즉 **active-low** 입력이다.
- D12 Red LED, D13 Blue LED는 이 실습 구성에서 **출력 HIGH일 때 ON**이다.
- D5는 **Active Buzzer**이므로 단순 경고음은 GPIO HIGH만으로도 낼 수 있고, 음악/톤은 GTM PWM으로 구동한다.
- A0 Potentiometer는 약 0 ~ 전원전압의 연속 전압을 만든다.
- A1 LDR 회로는 현재 실습 보드 기준으로 **밝아질수록 ADC 값이 커지고, 어두워질수록 작아지는 방향**으로 확인되어 있다.

> ShieldBuddy 공식 매핑 표에서는 A0=`SAR4.7/P32.3`, A1=`SAR4.6/P32.4`, D2=`P2.0`, D3=`P2.1`, D5=`P2.3`, D9=`P2.7`, D10=`P10.5`, D11=`P10.3`, D12=`P10.1`, D13=`P10.2`로 연결된다.

---

## 3. TC275의 메모리 맵 I/O를 읽는 법

이 실습은 OS 드라이버를 거치지 않고 **Memory-Mapped I/O(MMIO)** 방식으로 주변장치 레지스터 주소에 직접 접근한다.

예를 들어 `P10.1` Red LED를 제어할 때:

```text
P10 Port module base = 0xF003B000
P10_IOCR0            = base + 0x10
P10_OMR              = base + 0x04
```

C에서는 아래처럼 특정 주소를 `volatile` 포인터로 해석한다.

```c
#define PORT10_BASE_ADDRESS  (0xF003B000u)
#define PORT10_IOCR0         (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10u))
#define PORT10_OMR           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04u))
```

`volatile`이 필요한 이유는 이 주소가 일반 RAM 변수가 아니라 **실제 하드웨어 상태와 연결된 레지스터**이기 때문이다. 컴파일러가 읽기/쓰기를 임의로 제거하거나 캐싱하면 안 된다.

---

## 4. PORT 모듈: GPIO 실습의 기본

### 4.1 실습에서 자주 쓰는 Port Base Address

| Port | Base address | 현재 실습에서 쓰는 핀 |
|---|---:|---|
| `P02` | `0xF003A200` | SW1, SW2, Buzzer, RGB Red |
| `P10` | `0xF003B000` | Red/Blue LED, RGB Green/Blue |
| `P32` | `0xF003D200` | A0/A1/A2 계열 analog pin |

TC275 PORT 모듈은 포트 하나당 0x100 byte 영역을 갖는다.

### 4.2 핵심 PORT 레지스터

| Register | Offset | 역할 |
|---|---:|---|
| `Pn_OUT` | `+0x00` | GPIO 출력값 자체 |
| `Pn_OMR` | `+0x04` | 특정 bit만 set / clear / toggle |
| `Pn_IOCR0` | `+0x10` | Pin 0~3 기능 설정 |
| `Pn_IOCR4` | `+0x14` | Pin 4~7 기능 설정 |
| `Pn_IOCR8` | `+0x18` | Pin 8~11 기능 설정 |
| `Pn_IOCR12` | `+0x1C` | Pin 12~15 기능 설정 |
| `Pn_IN` | `+0x24` | 현재 입력 레벨 읽기 |
| `Pn_PDR0/1` | `+0x40/+0x44` | Pad driver 특성 |
| `Pn_PDISC` | `+0x60` | Pin function decision / analog 관련 설정 |

### 4.3 IOCR의 PCx 필드 위치

`IOCR0` 하나가 4개 핀을 관리한다.

| Pin | PC field bit 위치 |
|---|---:|
| Pin 0 | `[7:3]` → shift `3` |
| Pin 1 | `[15:11]` → shift `11` |
| Pin 2 | `[23:19]` → shift `19` |
| Pin 3 | `[31:27]` → shift `27` |

`IOCR4`도 같은 방식으로 Pin 4~7을 관리한다.

실습에서 자주 쓰는 `PCx[4:0]` 값:

| 값 | 의미 |
|---:|---|
| `0x00` | GPIO input, tri-state |
| `0x01` | GPIO input + pull-down |
| `0x02` | GPIO input + pull-up |
| `0x10` | push-pull GPIO output |
| `0x11` | push-pull alternate output function 1 |
| `0x12` ~ `0x17` | alternate output function 2~7 |
| `0x18` | open-drain GPIO output |

예: SW1 `P02.0`을 pull-up input으로 만들기:

```c
PORT2_IOCR0 &= ~(0x1Fu << 3u);
PORT2_IOCR0 |=  (0x02u << 3u);
```

예: Red LED `P10.1`을 GPIO output으로 만들기:

```c
PORT10_IOCR0 &= ~(0x1Fu << 11u);
PORT10_IOCR0 |=  (0x10u << 11u);
```

### 4.4 OMR: 출력 bit 하나만 안전하게 바꾸기

`Pn_OMR`은 아래처럼 사용한다.

- `PSx` = bit `x` : 해당 출력 set → HIGH
- `PCLx` = bit `x + 16` : 해당 출력 clear → LOW
- 같은 핀의 `PSx`와 `PCLx`를 동시에 1로 쓰는 패턴은 toggle에 사용 가능

예: `P10.1` Red LED ON/OFF:

```c
/* ON */
PORT10_OMR = (1u << 1u);

/* OFF */
PORT10_OMR = (1u << 17u);
```

`OUT` 전체를 read-modify-write 하는 것보다 `OMR`을 쓰면 다른 핀을 건드릴 위험이 작다. 그래서 종합 예제들은 OMR 방식이 많다.

---

## 5. 스위치 입력과 외부 인터럽트(ERU)

### 5.1 SW1/SW2 GPIO 입력

- SW1: `D2 → P02.0`
- SW2: `D3 → P02.1`
- 스위치 회로는 누르면 GND로 당겨지므로 **pressed = 0, released = 1**

따라서 보통 내부 pull-up을 설정한다.

```c
/* P02.0, P02.1: pull-up input */
PORT2_IOCR0 &= ~((0x1Fu << 3u) | (0x1Fu << 11u));
PORT2_IOCR0 |=  ((0x02u << 3u) | (0x02u << 11u));
```

현재 레벨은 `P02_IN` (`base + 0x24`)에서 읽는다.

### 5.2 현재 저장소에서 검증된 SW1 인터럽트 경로

[`05_interrupt_red_blue.c`](./05_interrupt_red_blue.c), [`05_interrupt_blue.c`](./05_interrupt_blue.c)에서 사용한 경로:

```text
Easy Module Shield SW1
D2
→ P02.0
→ SCU REQ6
→ ERS3 input(In32)
→ ETL3
→ OGU0
→ SRC_SCU_ERU0
→ CPU0 ISR
```

핵심 레지스터:

| Register | Address 구성 | 역할 |
|---|---|---|
| `SCU_EICR1` | `0xF0036000 + 0x214` | ERU input 선택 / edge 검출 / event enable |
| `SCU_IGCR0` | `0xF0036000 + 0x22C` | ETL → OGU 출력 연결 |
| `SRC_SCU_ERU0` | `0xF0038000 + 0xCD4` | CPU service request priority/enable/target |

현재 동작 코드의 핵심 설정:

```text
EXIS1 = 2      : P02.0 쪽 In32 선택
FEN1  = 1      : falling edge 검출 (스위치 누름)
EIEN1 = 1      : trigger event generation enable
INP1  = 0      : OGU0로 전달
IGP0  = 1      : trigger가 오면 IOUT0 생성
SRPN  = 0x0F   : interrupt priority
SRE   = 1      : service request enable
TOS   = 0      : CPU0 target
```

> D3/SW2도 ShieldBuddy에서 interrupt-capable pin이지만, **이 저장소에서 direct-register 방식으로 검증한 구체 ERU 배선은 우선 SW1/D2 경로를 기준으로 삼는다.**

---

## 6. STM(System Timer): 주기/시간 측정

TC275에는 STM0/STM1/STM2가 있고, 현재 bare-metal 실습은 주로 **STM0**을 사용한다.

### 6.1 STM0 주요 주소

```text
STM0 base  = 0xF0000000
TIM0       = base + 0x10
CMP0       = base + 0x30
CMCON      = base + 0x38
ICR        = base + 0x3C
ISCR       = base + 0x40
```

STM0 interrupt source:

```text
SRC base    = 0xF0038000
STM0_SR0    = SRC base + 0x490
```

현재 [`06_timer_interrupt.c`](./06_timer_interrupt.c)에서는 STM0을 **100 MHz** 기준으로 사용한다.

```c
#define STM0_FREQUENCY_HZ 100000000UL
#define STM0_TICKS_1SEC   STM0_FREQUENCY_HZ
```

1초 주기 예제의 흐름:

```text
TIM0 현재값 읽기
→ CMP0 = TIM0 + 100,000,000
→ CMP0 compare enable
→ compare match
→ SRC_STM0_SR0
→ CPU0 ISR
→ CMP0 += 100,000,000
→ 다음 1초 예약
```

종합 차량 예제에서는 interrupt 대신 `STM0_TIM0`의 차이를 직접 읽어 500 ms blink 시간을 만든다.

```c
if ((unsigned int)(now - last_tick) >= 50000000u)
{
    /* 500 ms elapsed */
}
```

unsigned subtraction 방식은 32-bit timer wrap-around 상황에서도 일반적인 짧은 시간차 계산에 유리하다.

---

## 7. GTM / TOM / PWM

GTM(Generic Timer Module)은 TC275에서 PWM, 캡처, 모터제어 같은 정밀 시간 신호를 담당하는 큰 타이머 블록이다.

실습의 PWM 흐름은 아래 순서로 외우면 된다.

```text
GTM module enable
→ CMU clock 선택/enable
→ TOM channel의 CTRL 설정
→ SR0(period), SR1(duty) 작성
→ TGC에서 update enable
→ channel/output enable
→ TOUT mux 연결
→ HOST_TRIG
→ 실제 CM0/CM1에 반영되어 PWM 출력
```

### 7.1 GTM 공통 레지스터

```text
GTM base          = 0xF0100000
GTM_CLC           = base + 0x9FD00
GTM_CMU_CLK_EN    = base + 0x00300
GTM_CMU_FXCLK_CTRL= base + 0x00344
```

`GTM_CLC`는 ENDINIT 보호 대상이므로 쓰기 전에 WDT ENDINIT 절차가 필요하다.

### 7.2 D12 Red LED PWM 경로

[`07_pwm.c`](./07_pwm.c)에서 사용:

```text
D12
→ P10.1
→ P10.1 alternate output function 1
→ TOUT103
→ GTM TOM0 Channel 1
```

핵심 레지스터:

```text
GTM_TOUTSEL6              = GTM base + 0x9FD48
GTM_TOM0_TGC0_GLB_CTRL    = GTM base + 0x08030
GTM_TOM0_TGC0_FUPD_CTRL   = GTM base + 0x08038
GTM_TOM0_TGC0_ENDIS_CTRL  = GTM base + 0x08070
GTM_TOM0_TGC0_OUTEN_CTRL  = GTM base + 0x08078
GTM_TOM0_CH1_CTRL          = GTM base + 0x08040
GTM_TOM0_CH1_SR0           = GTM base + 0x08044
GTM_TOM0_CH1_SR1           = GTM base + 0x08048
```

현재 코드에서는 `CMU_FXCLK1 = 6.25 MHz`를 선택하고 예를 들어:

```c
SR0 = 12500;   /* period -> 500 Hz */
SR1 = 6250;    /* 50% duty */
```

처럼 사용한다.

### 7.3 D5 Buzzer PWM 경로

종합 예제 [`09_vehicle_system.c`](./09_vehicle_system.c)와 음악 예제 계열에서 확인한 경로:

```text
D5 Active Buzzer
→ P02.3
→ alternate output function 1
→ TOUT3
→ GTM TOM0 Channel 11
```

주요 레지스터:

```text
GTM_TOUTSEL0              = GTM base + 0x9FD30
GTM_TOM0_TGC1_GLB_CTRL    = GTM base + 0x08230
GTM_TOM0_TGC1_FUPD_CTRL   = GTM base + 0x08238
GTM_TOM0_TGC1_ENDIS_CTRL  = GTM base + 0x08270
GTM_TOM0_TGC1_OUTEN_CTRL  = GTM base + 0x08278
GTM_TOM0_CH11_CTRL         = GTM base + 0x082C0
GTM_TOM0_CH11_SR0          = GTM base + 0x082C4
GTM_TOM0_CH11_SR1          = GTM base + 0x082C8
GTM_TOM0_CH11_CM1          = GTM base + 0x082D0
```

단순 경고음이면 active buzzer라 GPIO HIGH로도 충분하지만, 음높이를 바꾸는 음악 재생은 TOM PWM 주파수를 바꿔야 한다.

---

## 8. VADC: A0 Potentiometer / A1 LDR 읽기

### 8.1 아날로그 핀과 VADC channel

| 실드 | TC275 analog path | 실제 실습 |
|---|---|---|
| A0 Potentiometer | `SAR4.7 / P32.3` | `VADC Group 4 Channel 7` |
| A1 LDR | `SAR4.6 / P32.4` | `VADC Group 4 Channel 6` |
| A2 LM35 | `SAR4.5 / P32.1` | `VADC Group 4 Channel 5` |
| A3 Analog In | `SAR4.4 / P23.2` | `VADC Group 4 Channel 4` |

A0/A1은 analog input 전용 경로를 사용하므로 LED처럼 `PORT_IOCR`을 GPIO input으로 별도 설정하는 방식이 아니라 VADC 채널 설정이 핵심이다.

### 8.2 VADC 주요 주소

```text
VADC base       = 0xF0020000
VADC_CLC        = base + 0x000
G4ARBCFG        = base + 0x1480
G4ARBPR         = base + 0x1484
G4ICLASS0       = base + 0x14A0
G4QMR0          = base + 0x1504
G4QINR0         = base + 0x1510
G4CHCTR6        = base + 0x1618   /* A1 */
G4CHCTR7        = base + 0x161C   /* A0 */
G4RES0          = base + 0x1700
G4RES1          = base + 0x1704
```

현재 저장소의 A0 단독 실습 [`08_adc.c`](./08_adc.c)는 Channel 7을 `G4RES1`에 연결한다.

종합 예제 [`09_vehicle_system.c`](./09_vehicle_system.c)는 두 채널을 동시에 쓰므로:

```text
CH6 (LDR A1)         → G4RES0
CH7 (Steering A0)    → G4RES1
```

로 분리한다.

### 8.3 변환 순서

현재 코드들의 구조를 줄이면:

```text
1. ENDINIT 해제
2. VADC_CLC에서 module enable
3. ENDINIT 복구
4. Group4 arbitration 설정
5. Queue source enable
6. Analog converter normal operation
7. Input class = 12-bit
8. Channel → Result register 연결
9. G4QINR0에 channel 번호 write
10. TREV software trigger
11. RESx의 VF(valid flag) 대기
12. bits[11:0] 읽기
```

12-bit ADC이므로 범위는 보통:

```text
0 ~ 4095
```

이며 중간값은 약 2048이다.

> `G4QINR0`는 현재 코드에서 **read-modify-write하지 않고 직접 write**한다. 같은 주소를 read할 때 queue status 의미가 겹치기 때문에 `|=` 방식으로 쓰지 않는 것이 중요하다.

---

## 9. ENDINIT / Watchdog 보호 레지스터

TC275의 일부 중요한 시스템 레지스터는 실수로 변경되는 것을 막기 위해 ENDINIT으로 보호된다.

현재 실습에서 대표적으로 보호를 풀어야 하는 것:

- `GTM_CLC`
- `VADC_CLC`
- 일부 System Critical Register

사용하는 레지스터:

```text
SCU base          = 0xF0036000
SCU_WDTCPU0CON0   = base + 0x100
LCK               = bit 1
ENDINIT           = bit 0
```

흐름은:

```text
Password Access로 WDTCPU0CON0 unlock
→ Modify Access로 ENDINIT=0
→ 보호 레지스터 변경
→ 다시 Password Access
→ ENDINIT=1
→ lock 복구
```

`07_pwm.c`, `08_adc.c`, `09_vehicle_system.c`에 실제 코드가 들어 있다.

> 실습 코드에서는 편의를 위해 CPU/Safety watchdog 자체를 disable하는 경우가 많다. 실제 차량용 제품 코드에서는 watchdog을 무작정 끄는 방식이 아니라 정상적으로 service하는 설계가 필요하다.

---

## 10. Interrupt Router의 SRC 레지스터 공통 패턴

TC275의 주변장치에서 interrupt event가 생겨도 바로 CPU로 가는 것이 아니라 SRC(Service Request Control)를 거친다.

실습에서 반복해서 보는 필드:

| Field | 위치 | 역할 |
|---|---:|---|
| `SRPN` | `[7:0]` | interrupt priority number |
| `SRE` | bit `10` | service request enable |
| `TOS` | `[12:11]` | 어떤 service provider/CPU로 보낼지 선택 |

예를 들어 STM0 compare와 ERU 외부 인터럽트 모두 마지막에는 SRC 설정이 필요하다.

```text
주변장치 event
→ peripheral interrupt flag
→ SRC_xxx
→ SRPN priority + SRE enable + TOS target
→ CPU interrupt controller
→ __interrupt(priority) ISR
```

---

## 11. 종합 Vehicle 실습을 하드웨어 관점에서 보기

[`09_vehicle_system.c`](./09_vehicle_system.c)는 앞 실습의 하드웨어 블록을 거의 다 합친 예제라 전체 구조를 보기 좋다.

### 11.1 입력

```text
SW1 D2 → P02.0 → GPIO IN
SW2 D3 → P02.1 → GPIO IN
A0 Pot → SAR4.7 → VADC G4 CH7
A1 LDR → SAR4.6 → VADC G4 CH6
STM0_TIM0 → blink timing
```

### 11.2 판단

```text
SW1/SW2 입력
+ steering ADC 영역
+ LDR ADC threshold
+ STM 시간차
→ turn state / headlamp state / warning state 결정
```

### 11.3 출력

```text
Left indicator  → D13 → P10.2
Right indicator → D12 → P10.1
Head lamp RGB   → D9/P02.7 + D10/P10.5 + D11/P10.3
Warning buzzer  → D5/P02.3 → GTM TOM0_CH11 PWM
```

즉 이 파일 하나에서 다음 실습들이 연결된다.

```text
GPIO output
+ GPIO input
+ OMR
+ STM timing
+ VADC
+ GTM/TOM PWM
+ ENDINIT
```

---

## 12. 기존 실습 파일과 하드웨어 주제 연결

| 파일 | 핵심 하드웨어/레지스터 |
|---|---|
| [`01_led.c`](./01_led.c) | P10 GPIO output |
| [`02_led_v2_1_red.c`](./02_led_v2_1_red.c) | LED GPIO 제어 |
| [`03_rgb_led.c`](./03_rgb_led.c) | P02/P10 RGB GPIO |
| [`04_switch.c`](./04_switch.c) | P02 input, pull-up, `Pn_IN` |
| [`05_interrupt.c`](./05_interrupt.c) | SCU ERU + SRC interrupt |
| [`05_interrupt_red_blue.c`](./05_interrupt_red_blue.c) | 검증된 D2/P02.0 ERU 경로 |
| [`06_timer_interrupt.c`](./06_timer_interrupt.c) | STM0 compare interrupt |
| [`07_pwm.c`](./07_pwm.c) | GTM/CMU/TOM0_CH1 → P10.1 PWM |
| [`07_pwm_control_3_buzzer_button.c`](./07_pwm_control_3_buzzer_button.c) | D5/P02.3 buzzer PWM |
| [`08_adc.c`](./08_adc.c) | A0 → VADC G4 CH7 |
| [`08_adc_brightness.c`](./08_adc_brightness.c) | ADC 값을 PWM 밝기로 연결 |
| [`09_turn_signals.c`](./09_turn_signals.c) | SW + A0 + STM + LED + Buzzer |
| [`09_vehicle_system.c`](./09_vehicle_system.c) | A0/A1 + SW + LED/RGB + STM + GTM 종합 |
| [`10_scale_interrupt_stop.c`](./10_scale_interrupt_stop.c) | PWM tone + interrupt stop 종합 |

---

## 13. 데이터시트에서 무엇을 찾아야 하는가

실습 코드를 새로 짤 때는 아래 순서로 찾으면 된다.

### 13.1 먼저 보드 배선표

질문:

```text
실드의 D12가 TC275의 무슨 핀인가?
```

→ `ShieldBuddy User Manual - Arduino To ShieldBuddy To TC275 Mapping`

예:

```text
D12 → P10.1
A0  → SAR4.7/P32.3
```

### 13.2 다음으로 해당 Port Pin Function 표

질문:

```text
P10.1에서 GTM PWM을 내보내려면 alternate function 몇 번인가?
```

→ TC27x User Manual의 `Port 10 Functions`

예:

```text
P10.1 GPIO output  = PC1 1X000B
P10.1 GTM TOUT103  = PC1 1X001B
```

즉 push-pull 기준:

```text
GPIO       → PC1 = 0x10
GTM ALT1   → PC1 = 0x11
```

### 13.3 그 다음 주변장치 블록

목적에 따라:

```text
GPIO          → PORT chapter
외부 interrupt → SCU ERU + SRC
주기 interrupt → STM + SRC
PWM           → GTM → CMU → TOM → TGC → TOUT
Analog input  → VADC → Group → Queue → Channel → Result
```

### 13.4 마지막으로 실제 레지스터의 Offset / Bit Field

예:

```text
P10 base + IOCR0 offset
GTM base + TOM0_CH1_SR0 offset
VADC base + G4CHCTR7 offset
```

그리고 각 bit field의 **폭, 위치, 값의 의미**를 확인한다.

---

## 14. 자주 하는 실수

1. **D12와 P10.1을 같은 계층의 핀 이름으로 생각함**
   - D12는 실드/Arduino 커넥터 이름, P10.1은 TC275 핀이다.

2. **IOCR만 설정하면 주변장치 PWM까지 자동 연결된다고 생각함**
   - `P10.1 = ALT1`만으로 끝이 아니라 GTM TOM과 TOUT mux도 설정해야 한다.

3. **스위치를 active-high로 읽음**
   - SW1/SW2는 눌렀을 때 LOW.

4. **OMR의 clear bit 위치를 `x`로 착각함**
   - set=`x`, clear=`x+16`.

5. **A0를 GPIO input처럼 IOCR 설정하려 함**
   - A0는 SAR4.7 VADC 경로가 핵심이다.

6. **VADC `QINR`에 `|=` 사용**
   - 현재 TC275 코드에서는 직접 write 방식 사용.

7. **GTM/VADC CLC를 ENDINIT 해제 없이 수정**
   - 보호 때문에 쓰기가 반영되지 않을 수 있다.

8. **PWM의 SR0/SR1만 쓰고 HOST_TRIG/update를 빼먹음**
   - Shadow register 값이 실제 compare register로 넘어가야 한다.

9. **실드 부품 종류를 착각함**
   - D5는 passive speaker가 아니라 현재 자료 기준 Active Buzzer다. 단순 ON/OFF 경고는 HIGH/LOW로도 가능하다.

---

## 15. 실습용 최소 암기표

```text
[GPIO]
P02 base = F003A200
P10 base = F003B000
OUT  +00
OMR  +04
IOCR0 +10
IOCR4 +14
IN   +24

PC field shift = 3, 11, 19, 27
0x02 = input pull-up
0x10 = GPIO push-pull output
0x11 = ALT1 push-pull output
OMR set bit   = x
OMR clear bit = x+16

[Shield]
D2  SW1       -> P02.0
D3  SW2       -> P02.1
D5  Buzzer    -> P02.3
D9  RGB R     -> P02.7
D10 RGB G     -> P10.5
D11 RGB B     -> P10.3
D12 Red LED   -> P10.1
D13 Blue LED  -> P10.2
A0  Pot       -> VADC G4 CH7 / P32.3
A1  LDR       -> VADC G4 CH6 / P32.4

[STM0]
base F0000000
TIM0 +10
CMP0 +30
CMCON +38
ICR +3C
ISCR +40

[VADC]
base F0020000
A0 = G4 CH7
A1 = G4 CH6

[GTM]
base F0100000
D12/P10.1 PWM = TOUT103 = TOM0_CH1
D5/P02.3 PWM  = TOUT3   = TOM0_CH11
```

---

## 16. 원자료 위치

- **MCU 프로그래밍 강의자료**
  - ShieldBuddy pin map: 약 p.39~45
  - Easy Module Shield: 약 p.49~53
  - TC275 memory map / PORT register: 약 p.61~75
  - Switch/ERU: p.115 이후
  - STM timer/interrupt: timer 단원
  - GTM/TOM PWM: PWM 단원
  - VADC / Potentiometer: ADC 단원 약 p.269 이후
- **ShieldBuddy TC275 User Manual**
  - Table 5 `Arduino To ShieldBuddy To TC275 Mapping`: p.44~45
  - connector diagram: p.41~46
  - power: p.47
- **YwRobot Easy Module Shield V1**
  - 부품 ↔ Arduino pin 표: p.1~2
- **Infineon AURIX TC27x D-Step User Manual**
  - PORTS / SCU / IR(SRC) / STM / GTM / VADC chapter의 register table과 pin function table이 최종 기준

이 문서는 **강의자료의 실습 흐름 + 공식 ShieldBuddy/TC275 자료 + 현재 저장소에서 실제 동작하도록 수정된 코드**를 함께 기준으로 정리했다.
