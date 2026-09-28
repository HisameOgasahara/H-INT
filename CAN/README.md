# CAN Python 실습 정리

자동차 통신 시스템_260927 교재의 **CAN HoT with CanKing and Python-CAN** 실습 파일이다.

## 실습 환경

- OS: Windows
- CAN 환경: **Kvaser Virtual CAN Channel**
- 프로그램: **Kvaser Device Guide**, **Kvaser CanKing**, Python
- Python 패키지:
  ```bash
  pip install python-can cantools
  ```
- Python CAN backend: `interface='kvaser'`
- 실습 코드의 통신 속도: 주로 **1,000,000 bit/s (1 Mbit/s)**
- 기본 구성: 수신은 주로 channel 0, 송신은 주로 channel 1
- 공통 DBC: `project.dbc`

교재에서는 Kvaser Virtual CAN Driver를 확인한 뒤 CanKing에서 가상 채널을 선택하고, Bus Speed를 **1000 kbit/s**로 설정하여 Message Trace로 송수신을 확인한다.

> DBC를 사용하는 예제는 `cantools.database.load_file('project.dbc')`처럼 상대경로로 읽으므로, 이 저장소에서는 `CAN` 폴더를 현재 작업 폴더로 두고 실행하는 편이 편하다. 예: `python 04/can_send_multi_dbc.py`

## 번호별 실습

### 01. 사용 가능한 CAN 채널 확인
파일: `01/can_list_channel.py`

`python-can`에서 Kvaser 인터페이스로 열 수 있는 CAN 채널을 탐색하고 출력한다.

### 02. 단일 CAN 메시지 송수신
파일: `02/can_send.py`  
공통 수신 파일: `can_receive.py`

송신 측은 표준 CAN ID `0x123` 메시지를 1초마다 보내고, 수신 측은 들어오는 메시지를 계속 읽어 출력한다. 루트의 `can_receive.py`는 교재의 이 기본 송수신 실습에서 함께 사용하는 공통 수신 코드다.

### 03. 여러 CAN 메시지 송신
파일: `03/can_send_multi.py`

여러 `can.Message` 객체를 리스트로 만든 뒤 순회하면서 차례대로 전송한다. Python list/loop와 CAN 다중 메시지 송신을 함께 연습한다.

### 04. DBC 기반 여러 메시지 송신
파일: `04/can_send_multi_dbc.py`  
공통 파일: `project.dbc`

`cantools`로 DBC를 읽고, DBC에서 sender가 `ECU1`인 메시지를 골라 각 signal 값을 만들고 `encode()`한 뒤 CAN frame으로 보낸다.

### 05. DBC Signal 속성 확인 + 송신
파일: `05/can_send_multi_dbc_siginfo.py`

04 실습에 signal의 `name`, `start`, `length`, `byte_order`, `minimum`, `maximum` 정보를 확인하는 과정을 추가한다.

### 06. DBC Cycle Time 기반 주기 송신 / Thread
파일:
- `06/can_send_multi_dbc_cycle.py`
- `06/can_send_multi_dbc_cycle_threads.py`

DBC에 정의된 `send_type`과 `cycle_time`을 읽어 메시지별 송신 주기를 적용한다. 두 번째 파일은 메시지별 송신을 Python thread로 분리하여 서로 다른 주기의 메시지를 병행 송신한다.

### 07. python-can periodic send
파일: `07/can_send_multi_dbc_cycle_periodic.py`

직접 `sleep()` 루프를 구성하는 대신 `python-can`의 `send_periodic()`을 사용해 DBC cycle time에 맞춰 주기 송신한다.

### 08. CAN Message Filter / Mask
파일: `08/can_receive_filters.py`

`can_id`와 `can_mask` 필터를 설정하고 조건에 맞는 CAN ID만 수신한다. CAN 수신 버퍼에서 필요한 메시지만 선별하는 방법을 연습한다.

### 09. 송신과 수신 동시 수행
파일: `09/can_send_receive.py`

`ThreadSafeBus`와 송신/수신 thread를 사용해 한 프로그램 안에서 CAN 메시지를 동시에 송수신한다. DBC 기반 송신, periodic send, 수신 filter를 한 번에 결합한 실습이다.

실행 시 CAN channel을 인자로 받는다.

```bash
python 09/can_send_receive.py 0
```

### 10. Remote Frame(RTR) Request / Response
파일:
- `10/can_request_remote.py`
- `10/can_response_remote.py`

Request 측에서 ID `0x123`의 Remote Frame(RTR)을 보내고, Response 측이 이를 수신하면 같은 ID의 Data Frame으로 실제 데이터를 돌려준다.

## 공통 파일

### `project.dbc`

04~09 실습에서 사용하는 CAN Database 파일이다. Message/Signal, 송신 ECU, Cycle Time 등의 통신 정의를 Python 코드에서 읽어 사용한다.

### `can_receive.py`

가장 기본적인 CAN 수신 루프다. 교재의 단일 메시지 송수신 단계에서 `02/can_send.py`와 짝으로 사용하며, 이후 실습의 수신 코드 구조를 이해하는 기준이 된다.
