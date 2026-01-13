# 02. RGB Color Mixer (ADC + PWM + Interrupt)

## 🎯 학습 목표
ADC로 가변 저항의 전압을 읽고, 그 값을 기반으로 PWM 신호를 생성하여 RGB LED의 색상과 밝기를 실시간으로 제어합니다. 또한, 이 과정을 FreeMASTER로 모니터링합니다.

## 1. Project Introduction (개요)

**S32K144 기반 실시간 RGB 컬러 믹서 제어 시스템**

본 프로젝트는 아날로그 센서 입력을 디지털 신호로 변환하고, 이를 바탕으로 액추에이터를 정밀 제어하는 임베디드 시스템의 핵심 메커니즘을 학습하기 위해 수행되었습니다. 특히 인터럽트 기반의 비차단형(Non-blocking) 설계를 통해 시스템 리소스 효율을 극대화하는 데 중점을 두었습니다.

* **목표**: ADC(가변저항) 입력을 통해 RGB LED의 밝기(PWM)를 실시간으로 제어하고, 인터럽트 기반의 비차단형 시스템 구현 역량 확보.
* **핵심 키워드**: `S32K144`, `ADC Interrupt`, `PWM Duty Cycle`, `Register Analysis`, `FreeMASTER`.

### 🧠 핵심 개념 잡기

#### 1. 인터럽트 (Interrupt)
CPU가 메인 루프(`while(1)`)를 돌고 있을 때, 특정 이벤트(ADC 완료, 타이머 종료 등)가 발생하면 **하던 일을 멈추고 즉시 전용 처리 함수(ISR)**를 실행하는 방식입니다.

* **비유:**
  * **폴링(Polling):** 배달이 왔는지 확인하려고 1분마다 현관문을 열어보는 것 (CPU 낭비 심함)
  * **인터럽트(Interrupt):** 배달이 오면 '초인종(Interrupt)'이 울릴 때까지 내 일을 하는 것 (효율적)
* **하드웨어 연결:** S32K144의 ADC 모듈은 변환이 완료되면 CPU에 인터럽트 신호를 보낼 수 있습니다.

#### 2. PWM (Pulse Width Modulation)
디지털 신호의 **'켜짐(High)'과 '꺼짐(Low)' 시간 비율**을 조절하여, LED에 전달되는 평균 전력을 제어하는 기술입니다. 전압을 직접 낮추지 않고도 밝기를 조절할 수 있습니다.

* **Duty Cycle (듀티 사이클):** 한 주기 동안 신호가 High인 비율입니다.
  $$Duty\ Cycle = \frac{T_{on}}{T_{period}} \times 100\%$$
* **RGB Mixing:** 빨강(R), 초록(G), 파랑(B) LED 각각의 PWM Duty Cycle을 다르게 주면 수만 가지의 색상을 조합할 수 있습니다.

---
## 2. System Architecture (시스템 구성)

시스템은 가변 저항의 전압 변화를 실시간으로 감지하여 LED의 각 채널(R, G, B)에 반영하는 구조로 설계되었습니다.

* **입력부 (Sensors)**: 가변저항(Potentiometer)이 ADC0_SE12 채널에 연결되어 $0\text{V} \sim 5\text{V}$의 전압을 $0 \sim 4095$의 디지털 수치로 변환합니다.
* **제어부 (MCU)**:
    * **ADC 모듈**: 변환 완료 시 인터럽트를 발생시켜 CPU에 데이터를 전달합니다.
    * **제어 로직**: 읽어들인 ADC 값을 기반으로 RGB 각 채널의 PWM Duty Cycle을 계산하여 색상을 조합합니다.
* **구동부 (Actuators)**: FTM0 모듈의 PWM 채널을 사용하여 RGB LED에 흐르는 평균 전력을 조절, 밝기와 색상을 출력합니다.
* **모니터링 (Monitoring)**: LPUART1 통신을 통해 FreeMASTER 툴로 현재 ADC 값과 PWM 출력 상태를 실시간 모니터링합니다.

---

## 3. Hardware Deep Dive (하드웨어 분석 및 설계)

### A. ADC0 (Analog-to-Digital Converter)
* **인터럽트 방식**: ADC는 구현 편의상 Polling 방식을 채택하였으나, FreeMASTER 통신을 위한 UART는 **인터럽트 핸들러(FMSTR_Isr)**를 직접 등록하여 사용함으로써 시스템 모니터링의 실시간성을 확보했습니다.
* **데이터 해상도**: 12-bit Resolution 설정을 통해 가변 저항의 미세한 회전도 정밀하게 감지합니다.

### B. FTM0 (FlexTimer Module) - PWM
* **Duty Cycle 제어**: $Duty\ Cycle = \frac{T_{on}}{T_{period}} \times 100\%$ 수식을 적용하여 LED의 밝기를 제어합니다.
* **Color Mixing**: 빨강, 초록, 파랑 채널의 Duty Cycle 조합을 통해 가변 저항의 위치에 따른 연속적인 색상 변화를 구현합니다.

---

## 4. Software Implementation (소프트웨어 로직)

본 프로젝트의 소프트웨어는 메인 루프 내에서의 데이터 획득(**Polling**)과 실시간 모니터링을 위한 통신 인터럽트(**Interrupt**)가 공존하는 하이브리드 구조로 설계되었습니다.

### 4.1. ADC 데이터 획득: Polling 방식
ADC 모듈은 가변 저항의 아날로그 전압을 읽기 위해 **폴링(Polling)** 방식을 사용합니다.

* **동작 원리**: `ADC_DRV_ConfigChan` 함수로 변환을 시작한 후, `ADC_DRV_GetConvCompleteFlag`가 `true`가 될 때까지 `while` 루프에서 대기하며 변환 완료 여부를 확인합니다.
* **설계 의도**: 단일 센서(가변 저항) 값을 읽는 단순한 구조에서 시스템 로직의 흐름을 직관적으로 파악하고, 데이터 획득 순서를 명확히 제어하기 위해 채택하였습니다.

### 4.2. Color Mixing Algorithm (색상 조합 로직)
ADC로 읽어들인 12비트 데이터($0 \sim 4095$)를 3개의 구간으로 나누어 RGB LED의 색상을 선형적으로 조절합니다.

| 구간 (ADC Range) | 제어 로직 (Duty Cycle 계산) | 활성화 LED |
| :--- | :--- | :--- |
| $0 \leq \text{Value} < 1365$ | $Duty_{Red} = \frac{Value \times 10000}{1365}$ | **Red** 가변 제어 |
| $1365 \leq \text{Value} < 2730$ | $Duty_{Green} = \frac{(Value - 1365) \times 10000}{1365}$ | **Green** 가변 제어 |
| $2730 \leq \text{Value} \leq 4095$ | $Duty_{Blue} = \frac{(Value - 2730) \times 10000}{1365}$ | **Blue** 가변 제어 |

* **PWM 업데이트**: 계산된 Duty 값은 `FTM_DRV_UpdatePwmChannel` 함수를 통해 실시간으로 LED의 밝기(평균 전력)에 반영됩니다.

### 4.3. 실시간 모니터링: UART Interrupt
데이터 획득과 달리, PC와의 통신(FreeMASTER)은 시스템의 실시간성을 보장하기 위해 **인터럽트(Interrupt)** 방식을 사용합니다.

* **ISR 등록**: `INT_SYS_InstallHandler` 함수를 사용하여 `LPUART1_RxTx_IRQn` 발생 시 FreeMASTER 전용 핸들러인 `FMSTR_Isr`이 즉각 실행되도록 설정하였습니다.
* **효과**: 메인 루프가 ADC 값을 처리하는 도중에도 PC의 데이터 요청(Interrupt)이 들어오면 하던 일을 멈추고 즉시 응답하므로, 데이터 끊김 없는 실시간 모니터링이 가능합니다.

---

## 5. Monitoring 결과 및 회고

### 5.1. FreeMASTER 실시간 모니터링 결과
LPUART1 인터럽트를 통해 S32K144 내부 데이터를 PC로 실시간 전송하여 시스템의 동작을 검증하였습니다.

* **실시간 데이터 시각화**: 가변 저항의 물리적 회전에 따라 `adcValue`가 $0 \sim 4095$ 범위에서 지연 없이 변화하는 것을 FreeMASTER Variable Recorder를 통해 확인하였습니다.
* **PWM 듀티 동기화**: ADC 값의 변화에 따라 `dutyRed`, `dutyGreen`, `dutyBlue` 변수가 설계된 알고리즘대로 상호 배타적으로 증가/감소하며 LED의 색상이 전환되는 파형을 관찰하였습니다.



### 5.2. 주요 디버깅 및 해결 과정 (Troubleshooting)
구현 과정에서 발생한 논리적, 설정상 문제를 다음과 같이 해결하며 임베디드 시스템에 대한 이해도를 높였습니다.

1. **FreeMASTER 통신 초기화 및 인터럽트 설정**
   * **문제**: FreeMASTER 엔진을 초기화(`FMSTR_Init`)했음에도 불구하고 PC 툴에서 데이터를 읽어오지 못하는 현상이 발생함.
   * **원인 분석**: LPUART1을 통해 데이터가 들어와도 CPU가 이를 처리할 인터럽트 서비스 루틴(ISR)이 연결되지 않아 통신 응답을 주지 못함.
   * **해결**: `INT_SYS_InstallHandler`를 사용하여 `LPUART1_RxTx_IRQn` 발생 시 FreeMASTER 라이브러리의 `FMSTR_Isr` 핸들러가 실행되도록 수동 등록하고, 인터럽트를 활성화하여 실시간 모니터링 기능을 복구함.

2. **ADC 데이터와 PWM 듀티 스케일링(Scaling) 최적화**
   * **문제**: ADC 결과값($0 \sim 4095$)을 PWM 듀티 범위($0 \sim 10000$)로 변환할 때, 정수 연산 과정에서 데이터 손실(Overflow/Truncation)이 발생하여 LED 밝기가 불연속적으로 변함.
   * **해결**: 연산 시 `uint32_t` 형변환(Casting)을 명시적으로 적용하여 중간 계산 과정에서 오버플로우를 방지하고, $1365$ 단위의 정확한 구간 분할을 통해 부드러운 색상 전환을 구현함.

### 5.3. 회고 (Retrospective)
* **하이브리드 방식의 이해**: ADC는 제어 루프 내에서 명확한 시점에 데이터를 읽기 위해 **Polling** 방식을 사용하고, 통신은 데이터의 불확실한 수신 시점에 즉각 대응하기 위해 **Interrupt** 방식을 사용하는 등, 각 방식의 장단점을 실제 코드에 적용하며 체득함.
* **하드웨어 제어 기반 확보**: 이번 프로젝트를 통해 습득한 ADC-PWM 연동 기술과 인터럽트 기반 모니터링 기법은 향후 센서 기반의 자동화 시스템(예: 스마트 와이퍼 시스템)을 설계하는 데 있어 핵심적인 기술적 자산이 됨.

---

## 🛠️ 하드웨어 구성
* **입력:** Potentiometer (ADC0_SE12) -> 전압을 0~4095 숫자로 변환.
* **출력:** RGB LED (FTM0 PWM 채널 활용)
* **모니터링:** FreeMASTER (LPUART1 활용)