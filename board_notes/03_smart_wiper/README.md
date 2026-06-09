# 03. Smart Wiper System (ADC + PWM + State Machine)

> 📂 **설계 사양서**
> [* **프로젝트 설계사양서(PPTX)**: `board_notes/03_smart_wiper/설계사양서(스마트 와이퍼 프로젝트).pptx`](https://docs.google.com/presentation/d/1ZYnYInHSKUaJvb6Nu8b6fTkJw7slTBsg455Bz-95eh8/edit?usp=sharing)

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

### 4.2. Servo Motor Control Algorithm (Updated)

서보모터(SG-90)는 PWM 신호의 **펄스 폭(Pulse Width, High 구간의 시간)**에 따라 회전 각도가 결정됩니다. 본 프로젝트에서는 S32K144의 FTM(FlexTimer) 모듈을 사용하여 이를 제어합니다.

#### **(1) PWM 및 타이머 설정 원리**

* **PWM 주기(Period):** 20ms (50Hz) — 서보모터의 표준 규격입니다.
* **타이머 계산 (Tick):** * 시스템 클럭 50MHz를 분주비(Prescaler) 32로 설정할 경우, 타이머는 1초에 약 1,562,500번 숫자를 셉니다. (1 Tick = 0.64us)
* 전체 주기 20ms를 채우기 위한 총 Tick 수: **약 31,250**


* **각도 제어 범위 (Pulse Width):** 서보모터는 전체 주기(31,250 틱) 중 약 2.5%~12.5% 범위의 짧은 신호만 인식합니다.

| 각도 (Angle) | 펄스 폭 (Time) | 타이머 값 (Ticks) | 코드 내 상수 |
| --- | --- | --- | --- |
| **** | **0.5 ms** | 약 781 | `POS_0_DEG (800)` |
| **** | **약 2.1 ms** | 약 3281 | `POS_140_DEG (3300)` |
| **** | **2.5 ms** | 약 3906 | - |

> **Note:** 0이나 31,250과 같은 극단적인 값을 사용하지 않는 이유는 서보모터가 20ms라는 전체 틀 안에서 특정 길이의 '신호'만 해독하여 각도를 결정하는 규격을 따르기 때문입니다.

#### **(2) 비차단 기반 시간 관리 (`moveDuration`)**

와이퍼의 속도 조절은 `moveDuration` 변수를 통해 제어하며, 이는 단순한 대기 시간이 아닌 **'물리적 이동 시간' + '도착 후 대기 시간'**의 합입니다.

* **동작 매커니즘:**
1. `FTM_DRV_UpdatePwmChannel`로 목표 각도 명령을 하달 (즉시 완료)
2. 모터가 목표치까지 물리적으로 이동 (약 0.2~0.25s 소요)
3. `moveDuration` 시간에서 이동 시간을 제외한 남은 시간 동안 목표 각도 유지 및 정지
4. 시간 초과 시 반대 방향으로 상태 전이 (State Transition)


* **모드별 속도 체감:**
* `HIGH` (300ms): 모터의 물리적 이동이 끝나자마자 거의 즉시 반대로 회전하여 가장 빠름.
* `LOW` (800ms): 모터가 도착한 후 약 0.5~0.6초간 정지해 있으므로 상대적으로 굼뜨게 느껴짐.

#### **(3) 구현 함수**

* `FTM_DRV_UpdatePwmChannel`: 계산된 Tick 값을 레지스터에 반영하여 PWM 듀티 사이클 변경.
* `LPIT0_Ch0_IRQHandler`: 1ms마다 발생하는 인터럽트로 `ms_ticks`를 증가시켜 소프트웨어 타이머 구현.


### 4.3 로직 개선: 딜레이(Blocking)에서 비차단(Non-blocking)으로

#### 1. 배경

초기 구현에서는 `OSIF_TimeDelay()` 함수를 사용하여 와이퍼의 동작 간격을 조절했습니다. 하지만 이 방식은 CPU를 해당 라인에서 멈추게 만들어, 실시간 데이터 시각화 도구인 **FreeMASTER와의 통신이 끊기는 문제**를 발생시켰습니다. 이를 해결하기 위해 상태 머신과 타이머 인터럽트를 이용한 **비차단 로직**으로 구조를 개선했습니다.

#### 2. 로직 비교

❌ 기존: 딜레이 로직 (Blocking Logic)

CPU가 `Delay` 함수에 갇혀 있는 동안에는 다른 어떤 작업(통신, 센서 읽기 등)도 수행할 수 없습니다.

```c
// [문제점] 함수 내부에서 CPU가 멈춤
void wipeOnce(uint32_t moveTime) {
    FTM_DRV_UpdatePwmChannel(..., maxDuty, ...);
    OSIF_TimeDelay(moveTime); // 여기서 CPU가 멈춤 (FreeMASTER 통신 단절)

    FTM_DRV_UpdatePwmChannel(..., minDuty, ...);
    OSIF_TimeDelay(moveTime); // 여기서도 CPU가 멈춤
}

// 메인 루프
for(;;) {
    FMSTR_Poll(); // 딜레이 때문에 호출 빈도가 급격히 낮아짐
    if (mode == MODE_INT) {
        wipeOnce(500);
        OSIF_TimeDelay(3000); // 3초간 통신 마비
    }
}

```

✅ 개선: 비차단 로직 (Non-blocking Logic)

1ms마다 발생하는 인터럽트(`ms_ticks`)를 시계로 삼아, 루프를 돌 때마다 "지금 움직일 시간인가?"를 체크합니다. CPU는 멈추지 않고 계속 `FMSTR_Poll()`을 호출합니다.

```c
// [해결] 시계(ms_ticks)를 확인하고 즉시 통과
for(;;) {
    FMSTR_Poll();     // 항상 빠르게 호출됨 (통신 유지)
    currentTime = ms_ticks;

    if (currentStep == WIPER_MOVING_UP) {
        // 시간이 다 되었을 때만 상태 변경, 아니면 바로 다음 코드로 진행
        if (currentTime - lastTime >= moveDuration) {
            currentStep = WIPER_MOVING_DOWN;
            lastTime = currentTime;
            FTM_DRV_UpdatePwmChannel(..., POS_0_DEG, ...);
        }
    }
    // ... 나머지 상태 머신 로직
}

```

#### 3. 주요 차이점 정리

| 항목 | 딜레이 로직 (기존) | 비차단 로직 (개선) |
| --- | --- | --- |
| **핵심 기법** | `OSIF_TimeDelay()` | 타이머 ISR + 상태 머신 (Switch-Case) |
| **CPU 상태** | 특정 구간에서 대기하며 공회전 | 매 루프마다 모든 조건을 체크하며 통과 |
| **FreeMASTER** | **통신 끊김 발생** (Timeout) | **안정적인 실시간 그래프 출력** |
| **시스템 반응성** | 딜레이 중에는 사용자 입력 무시 | 언제든 모드 변경 및 제어 가능 |

#### 4. 구현 핵심 (S32K144)

* **LPIT(Low Power Iterrupt Timer)**: 1ms 주기로 `ms_ticks` 변수를 증가시키는 인터럽트 서비스 루틴(ISR) 구현.
* **State Machine**: 와이퍼의 동작을 `IDLE`, `MOVING_UP`, `MOVING_DOWN`으로 세분화하여 각 단계에서 시간 조건 충족 시에만 다음 단계로 전이.
* **Control Authority**: `controlSource` 변수를 도입하여 가변저항(ADC) 제어와 PC(FreeMASTER) 원격 제어권을 분리.




- [ ] 인터럽트 & DMA 로 가는 설명 넣어야할듯
- [ ] 프로젝트에서 사용된 기본 이론 정리
- [ ] bare-metal 구현한 것
