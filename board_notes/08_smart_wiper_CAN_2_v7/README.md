# 📑 Project: SmartWiper_CAN_2_v7
**"Multi-Node Distributed Control & Intelligent Diagnostic System"**

## 🎯 프로젝트 개요
본 프로젝트는 단일 보드 검증 단계(v6)를 넘어, **두 개의 S32K144 EVB를 CAN 네트워크로 연결**하여 실시간 제어와 진단이 동시에 이루어지는 분산 제어 시스템을 구축하는 것을 목표로 합니다. 단순히 명령을 전달하는 것을 넘어, 시스템의 안전(Failsafe)과 코드의 재사용성(Layered Architecture)을 극대화한 산업용 수준의 소프트웨어를 지향합니다.

---

## 🚀 SmartWiper v7: Dual-Node Bidirectional System 계획
v7의 핵심은 **"역할 분담과 상호 감시"**입니다. 단순히 데이터를 주는 것에 그치지 않고, 잘 받았는지 혹은 현재 모터 상태가 어떤지 보드 B가 보드 A에게 다시 보고하는 구조를 만들 것입니다.

### 1단계: v6 코드의 레이어 분리 (Software Layering)
하드웨어를 연결하기 전에, `main.c`에 뭉쳐 있는 로직을 3개 층으로 쪼개서 **"어느 보드에서도 돌아갈 수 있는 범용 코드"**를 만듭니다.

* **APP (Application)**: 와이퍼 모드 결정 로직, 필터링 로직 (`wiper_app.c`)
* **HAL (Hardware Abstraction)**: `Get_ADC_Value()`, `Set_Motor_PWM()`, `Send_CAN_Msg()` 등 하드웨어 독립적인 인터페이스 (`wiper_hal.c`)
* **MCAL (Microcontroller Abstraction)**: 실제 SDK 함수나 레지스터 제어 (`flexcan_hw.c`, `adConv1.c` 등)

### 2단계: 하드웨어 배선 및 물리적 연결 (Physical Connection)
* **준비물**: EVB 보드 2개, CAN Transceiver(보드 내장), 점퍼 와이어 2개(H, L).
* **연결**: 보드 A의 `CAN_H` ↔ 보드 B의 `CAN_H`, 보드 A의 `CAN_L` ↔ 보드 B의 `CAN_L`.
* **주의**: 두 보드의 GND를 연결하여 전위차를 맞추고, 종단 저항(120Ω) 설정을 확인합니다.

### 3단계: 보드별 역할 정의 및 양방향 통신 시나리오
(보드 간 구체적인 통신 절차 및 에러 처리 시나리오가 진행될 예정입니다.)

---

## 🏗️ 시스템 아키텍처 (System Architecture)
본 시스템은 **Controller(Node A)**와 **Actuator(Node B)**가 120Ω 종단 저항이 포함된 CAN 버스로 연결된 양방향 피드백 구조입니다.



* **Node A (Controller)**: 가변저항(ADC) 입력 및 사용자 버튼(SW) 제어, UART를 통한 FreeMASTER 모니터링 수행.
* **Node B (Actuator)**: CAN 명령에 따른 서보 모터(FTM/PWM) 구동 및 자가 진단(Error/Status) 데이터 송신.

---

## 🛠️ 소프트웨어 계층화 (Layered Architecture)
변경에 유연하고 유지보수가 쉬운 시스템을 위해 3계층 아키텍처를 적용합니다.



1.  **Application Layer (`App_Wiper.c`)**:
    * 와이퍼 모드 전환 로직 (Auto/Manual).
    * 수동 트리거(1회 왕복) 시퀀스 제어.
2.  **Service Layer / Middleware (`CAN_Mgr.c`, `Filter.c`)**:
    * CAN 메시지 패킹/언패킹 및 ID 기반 중재 로직.
    * DSP: ADC 노이즈 제거를 위한 이동 평균 필터(Moving Average Filter).
    * Failsafe: 통신 타임아웃 감시(Watchdog) 및 센서 이상 진단.
3.  **HAL/MCAL Layer (`flexcan_hw.c`, `adc_hw.c`, `ftm_hw.c`)**:
    * S32K SDK 기반 하드웨어 레지스터 제어.
    * Normal 모드 CAN 통신 인터페이스 제공.

---

## 📡 통신 프로토콜 (CAN Protocol Design)

| CAN ID | 우선순위 | 송신 노드 | 데이터 필드 (Data Field) | 목적 |
| :--- | :--- | :--- | :--- | :--- |
| **0x050** | **Highest** | Node B | `[Error_Code][Reserved...]` | 긴급 중단 (Emergency Stop) |
| **0x100** | Medium | Node A | `[Control_Flag][ADC_H][ADC_L]` | 제어 명령 (Auto/Manual/Trigger) |
| **0x200** | Low | Node B | `[Status_Flag][Current_Step]` | 상태 보고 (Busy/Cycle Done) |

---

## 🛡️ Failsafe & Diagnostic 로직
시스템 신뢰성 확보를 위한 4단계 안전 장치를 구현합니다.

1.  **Communication Loss (보드 간 단선)**:
    * Node B가 Node A의 주기적 메시지(`0x100`)를 500ms 이상 수신하지 못할 경우, 서보 모터를 즉시 안전 위치(0도)로 복귀시킴.
2.  **Sensor Fault (가변저항 이상)**:
    * ADC 값이 범위를 벗어난 플로팅(Floating) 발생 시 Node A가 스스로 감지하여 CAN 버스에 에러 전파.
3.  **Mechanical Jam (모터 끼임)**:
    * Node B의 버튼 입력을 '기계적 부하'로 가정하여, 감지 시 즉시 전력을 차단하고 Node A에게 Emergency(0x050) 보고.
4.  **System Watchdog**:
    * 메인 루프 멈춤 감지 시 시스템을 리셋하여 무한 루프 방지.

---

## 🚀 사용자 인터페이스 (Node A 버튼 기능)
* **SW2 (Button 1)**: 자동(Auto) / 수동(Manual) 모드 전환.
* **SW3 (Button 2)**: 수동 모드 시 와이퍼 1회 왕복 트리거.

---

## 💡 학습 포인트 및 향후 계획
* CAN의 Multi-Master 특성을 활용한 양방향 통신 구현.
* 실제 차량 통신 규격(Motorola Endianness)에 맞춘 데이터 정렬.
* **Next Step**: CAN-FD로의 업그레이드 및 다중 노드(3개 이상의 보드) 네트워크 확장.
