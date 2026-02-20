# 03. Smart Wiper System (ADC + PWM + State Machine)

## 🎯 프로젝트 개요
S32K144EVB 보드와 외부 하드웨어(서보모터, 가변저항)를 연동하여, 빗물의 양을 감지하고 이에 따라 와이퍼의 속도를 자동으로 조절하는 **지능형 와이퍼 제어 시스템**을 구현합니다.

## 🛠️ 하드웨어 구성품
본 프로젝트는 S32K144EVB 보드 외에 다음과 같은 외부 부품을 사용합니다.
* **MCU**: NXP S32K144EVB-Q100
* **서보모터**: [SMG] TowerPro 호환 9g 미니 서보모터 SG-90
* **가변저항**: [NW3] 아두이노 가변 저항 10KΩ (다이얼 타입)
* **브레드보드**: [SMG] 브레드보드 400핀 Half Size [SZH-BBAD-005]
* **점퍼선**: [KEYES] CH254 소켓 점퍼 케이블 (M/M, M/F 세트)

## 📍 하드웨어 배선 계획 (Wiring)
| 부품 | 핀 이름 | EVB 보드 핀 | 케이블 타입 | 비고 |
| :--- | :--- | :--- | :--- | :--- |
| **공통 전원** | 5V (VCC) | **J3-09** | M/F | 브레드보드 (+) 레일 연결 |
| **공통 접지** | GND | **J3-11** | M/F | 브레드보드 (-) 레일 연결 |
| **가변저항** | Signal (Center) | **J4-05 (PTB0)** | M/F | ADC0_SE4 (빗물 양 감지) |
| **서보모터** | Signal (Orange) | **J1-01 (PTD15)** | M/F | FTM0_CH0 (와이퍼 구동 PWM) |

## 🧠 주요 학습 및 구현 목표
1. **ADC-PWM 연동 제어**: 가변저항의 아날로그 값(0~4095)을 읽어 서보모터의 회전 속도 및 주기를 가변적으로 제어합니다.
2. **State Machine 로직**: 빗물 양(ADC 값)에 따른 4단계 동작 모드 구현
    * **OFF**: 와이퍼 정지
    * **INT (Intermittent)**: 간헐적 동작 (느린 주기)
    * **LOW**: 저속 연속 동작
    * **HIGH**: 고속 연속 동작
3. **서보모터 정밀 제어**: PWM 듀티 사이클 계산을 통해 와이퍼의 왕복 각도를 0°~140° 범위 내로 정밀하게 제한합니다.
4. **FreeMASTER 모니터링**: 현재 동작 모드와 빗물 감지 수치를 실시간으로 시각화합니다.

## 📅 개발 로그
- [x] 하드웨어 부품 주문 및 수령
- [x] 브레드보드 물리 배선 및 전원 테스트
- [x] ADC 및 PWM 드라이버 설정 (S32DS)
- [x] 모드 전환 로직 및 서보모터 구동 코드 작성
- [x] FreeMASTER 인터페이스 구성 및 최종 디버깅

## 3. System Architecture (시스템 구성)

본 프로젝트는 **입력(Sensor) - 제어(Controller) - 출력(Actuator)**의 전형적인 임베디드 제어 루프를 따릅니다.

1.  **Input (Rain Sensor Simulation):** 가변저항(Potentiometer)을 통해 빗물의 양을 아날로그 전압($0 \sim 5V$)으로 모사합니다.
2.  **Processing (MCU):**
    *   **ADC:** 아날로그 전압을 디지털 값($0 \sim 4095$)으로 변환합니다.
    *   **State Machine:** 빗물의 양에 따라 와이퍼의 동작 모드(OFF/INT/LOW/HIGH)를 결정합니다.
3.  **Output (Wiper Motor):** 결정된 모드에 맞춰 서보모터의 각도를 제어하기 위한 PWM 신호를 생성합니다.

## 4. Software Implementation (소프트웨어 로직)

### 4.1. Finite State Machine (FSM) 설계
빗물 감지량(ADC 값)에 따라 4가지 상태로 구분하여 와이퍼를 제어합니다.

| 모드 (State) | 조건 (ADC Range) | 동작 설명 |
| :--- | :--- | :--- |
| **OFF** | $0 \sim 1000$ | 와이퍼 정지 (Parking Position, 0°) |
| **INT** (Intermittent) | $1001 \sim 2000$ | 간헐적 동작 (일정 주기마다 1회 왕복) |
| **LOW** | $2001 \sim 3000$ | 저속 연속 동작 |
| **HIGH** | $3001 \sim 4095$ | 고속 연속 동작 |

### 4.2. Servo Motor Control Algorithm
서보모터(SG-90)는 PWM 신호의 **Duty Cycle(High 구간의 시간)**에 따라 회전 각도가 결정됩니다.

*   **PWM 주기:** 20ms (50Hz)
*   **각도 제어:**
    *   $0^\circ \rightarrow 0.5ms$ (Duty 2.5%)
    *   $90^\circ \rightarrow 1.5ms$ (Duty 7.5%)
    *   $180^\circ \rightarrow 2.5ms$ (Duty 12.5%)
*   **구현:** S32K144의 FTM0 모듈을 사용하여 정밀한 PWM 파형을 생성하고, `FTM_DRV_UpdatePwmChannel` 함수로 듀티를 실시간 업데이트합니다.

[] 딜레이로직 -> 비차단로직 -> 인터럽트 & DMA 로 가는 설명도 넣어야할듯
[] 프로젝트에서 사용된 기본 이론 정리
[] bare-metal 구현한 것