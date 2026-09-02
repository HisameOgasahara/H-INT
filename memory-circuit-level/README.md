# 메모리 읽기/쓰기의 회로 수준 동작

이 폴더는 CPU가 메모리의 값을 읽고 쓰는 과정을 **주소선, 제어선, 워드라인, MOSFET, DRAM 1T1C 셀, 비트라인, 센스 앰프, 데이터 버스, CPU 레지스터, 클록**까지 내려가서 연결해 보기 위한 학습 자료다.

인터랙티브 HTML: [`dram_2bit_address_full_continuous.html`](./dram_2bit_address_full_continuous.html)

## 1. 가장 먼저 잡을 전체 흐름

CPU에서 `LOAD`가 실행된다고 생각하면 회로 수준에서는 다음 순서로 이어진다.

**주소 선택 → 주소선의 전압 패턴 → 디코더 → 특정 WL 선택 → access MOSFET ON → 선택된 DRAM 셀과 bit line 연결 → 전하 공유 → sense amplifier 판정 → data bus → CPU register → clock edge에서 저장**

여기서 전자가 주소를 알고 특정 셀까지 이동하는 것이 아니다. **주소 비트가 만든 전압 패턴이 MOSFET 논리회로의 ON/OFF 상태를 바꾸고, 그 결과 필요한 전기적 경로만 연결된다.**

## 2. 주소는 물리적으로 무엇인가

HTML에서는 주소를 2비트 `A1 A0`로 단순화했다.

- `00` → `WL0`
- `01` → `WL1`
- `10` → `WL2`
- `11` → `WL3`

`A1`, `A0`도 결국 배선의 전압이다. 논리 1은 높은 전압 범위, 논리 0은 낮은 전압 범위로 표현된다.

이 두 신호가 **2→4 row decoder**로 들어간다. 디코더 내부도 NOT/AND에 해당하는 MOSFET 네트워크이며, 주소 조합에 따라 네 출력 중 하나만 활성화된다.

예를 들어 `A1A0 = 10`이라면 논리적으로는 `WL2`만 선택된다.

## 3. Word Line은 무엇을 하는가

Word Line(WL)은 데이터를 운반하는 선이 아니다. **해당 행의 access MOSFET gate를 제어하는 선**이다.

선택된 WL의 전압이 올라가면 그 행의 access MOSFET gate 전압이 올라간다. Gate 전압이 임계값을 넘으면 MOSFET의 source-drain 사이가 전도 상태가 되어 저장 셀과 bit line 사이의 통로가 열린다.

즉,

**WL High → access MOSFET ON → 선택 셀 ↔ bit line 연결**

이다.

다른 행의 WL은 Low이므로 access MOSFET이 OFF이고 bit line과 분리된 상태를 유지한다.

## 4. DRAM 1T1C 셀은 무엇을 저장하는가

DRAM의 1비트 셀을 가장 단순하게 보면 다음 두 부품이다.

- `1T`: access MOSFET 1개
- `1C`: storage capacitor 1개

저장 자체는 **capacitor의 전하 상태**가 담당한다.

- 충분히 충전된 상태 → 논리 1로 해석
- 낮은 전하 상태 → 논리 0으로 해석

access MOSFET은 이 값을 저장하는 부품이 아니라 **읽거나 쓸 때만 capacitor를 bit line과 연결하는 스위치**다.

## 5. 읽기: capacitor의 전하가 어떻게 CPU까지 오는가

### 5.1 Bit line precharge

읽기 전 bit line은 미리 기준 전압, 개념적으로 `VDD/2` 부근으로 맞춰둔다.

이유는 저장값이 1인지 0인지에 따라 bit line이 기준보다 조금 위 또는 아래로 움직이는 것을 감지하기 쉽게 만들기 위해서다.

### 5.2 선택 셀과 bit line 연결

주소가 디코더를 통과하여 선택 WL이 High가 되면 access MOSFET이 열린다.

그러면

**storage capacitor ↔ access MOSFET ↔ bit line**

경로가 생긴다.

### 5.3 전하 공유

capacitor와 bit line은 둘 다 전기적으로 capacitance를 가진다. 둘이 연결되면 전하가 재배치되면서 bit line 전압이 조금 변한다.

저장값이 1이면 bit line이 기준 전압보다 조금 올라가고, 저장값이 0이면 조금 내려가는 식으로 이해하면 된다.

HTML의 파란 점은 이 **전하 재배치 방향을 시각화한 교육용 표현**이다. 개별 전자의 실제 궤적을 그대로 계산한 물리 시뮬레이션은 아니다.

### 5.4 Sense amplifier

bit line의 변화는 처음에는 매우 작다. 이 상태를 바로 CPU의 0/1로 사용하기 어렵다.

그래서 sense amplifier가 작은 전압 차이를 받아 **확실한 논리 0 또는 1 전압 수준**으로 증폭한다.

### 5.5 Data bus

메모리 출력이 유효해지면 선택된 메모리만 data bus를 구동한다.

개념적으로는 다음 조건을 생각할 수 있다.

`Output Enable = Chip Select AND Read Enable`

이 조건을 만족할 때만 메모리 출력 드라이버가 data bus에 0/1 전압을 건다. 선택되지 않은 장치는 버스를 방해하지 않도록 높은 임피던스 상태로 빠진다.

### 5.6 CPU register

Data bus의 값이 CPU까지 도착한 뒤, register 입력 회로가 그 값을 받는다.

CPU register도 결국 MOSFET으로 구성된 latch/flip-flop 계열 저장 회로다. HTML에서는 **clock-controlled input + cross-coupled inverter latch** 형태로 단순화했다.

클록의 특정 edge에서 입력 경로가 열리고, 충분히 안정된 data bus의 값이 latch 내부 상태로 저장된다.

## 6. 왜 시간이 필요한가

주소를 출력한 순간 모든 회로가 동시에 최종 상태가 되는 것이 아니다.

다음 과정 각각에 시간이 걸린다.

**주소선 전압 변화 → decoder propagation → WL 배선 충전 → access MOSFET 전도 증가 → capacitor/bit-line 전하 공유 → sense amplifier 증폭 → data bus 안정화 → CPU register capture**

배선과 MOSFET gate도 capacitance를 가지므로 전압은 순간적으로 바뀌지 않고 충전/방전 시간을 가진다.

그래서 클록은 단순히 "기다리는 타이머"라기보다, **회로 상태 변화의 순서를 맞추고 어느 시점의 값을 다음 저장소가 확정해서 받을지 정하는 공통 시간 기준**으로 이해하는 것이 좋다.

HTML 아래 그래프의 `CLK`, 선택된 `WL`, `BL`, `DATA`는 위 애니메이션과 같은 내부 시간 모델을 사용하며, 현재 시점까지의 파형만 순차적으로 그려진다.

## 7. 쓰기 동작은 어떻게 다른가

쓰기에서도 주소 선택까지는 동일하다.

**CPU 주소 출력 → decoder → WL 선택 → access MOSFET ON**

그 뒤 방향이 반대가 된다.

CPU가 data bus에 쓸 0/1을 내보내고, `CS + WR` 조건이 만족되면 메모리의 write driver가 bit line을 원하는 전압으로 강하게 구동한다.

그 결과

**data bus → write driver → bit line → access MOSFET → storage capacitor**

방향으로 전하 상태가 바뀐다.

- 1 쓰기: capacitor를 높은 전하 상태로 만듦
- 0 쓰기: capacitor를 낮은 전하 상태로 만듦

즉 읽기에서는 **셀의 작은 전압 변화를 감지하고 증폭**하는 것이 중요하고, 쓰기에서는 **외부 드라이버가 bit line을 충분히 강하게 밀어 셀의 전하 상태를 바꾸는 것**이 중요하다.

## 8. CS, RD, WR, 주소 버스, 데이터 버스의 역할

- **Address bus**: 어디를 접근할 것인가
- **CS (Chip Select)**: 여러 메모리/장치 중 누가 이번 요청에 응답할 것인가
- **RD / Read Enable**: 읽기 경로를 활성화할 것인가
- **WR / Write Enable**: 쓰기 경로를 활성화할 것인가
- **Word Line**: 선택된 메모리 내부에서 어느 행의 access MOSFET을 열 것인가
- **Bit Line**: 셀과 sense/write 회로 사이의 내부 데이터 경로
- **Data bus**: 메모리와 CPU 사이에서 최종 0/1 데이터를 전달하는 경로
- **Clock**: 어느 시점에 값을 유효한 상태로 받아 다음 저장 회로에 확정할 것인가

## 9. 이 HTML에서 의도적으로 단순화한 부분

이 HTML의 목적은 실제 DRAM 칩 전체를 SPICE 수준으로 재현하는 것이 아니라, **추상적인 LOAD/STORE가 전기회로 수준의 선택·전하 이동·증폭·타이밍으로 어떻게 내려가는지 연결해서 보는 것**이다.

따라서 다음은 교육용으로 단순화되어 있다.

- 실제 DRAM의 복잡한 row/column 구조
- differential bit-line pair와 실제 sense amplifier 세부 회로
- DRAM refresh, restore 과정
- memory controller의 실제 명령/타이밍 프로토콜
- CPU cache와 DRAM 사이의 여러 계층
- 실제 transistor parameter와 RC 값을 사용한 정량적 회로 시뮬레이션

그래도 핵심 연결은 유지한다.

**주소 전압 → MOSFET 논리 선택 → WL → access MOSFET → capacitor/bit line → sense/write driver → data bus → CPU register → clocked state transition**
