# 📘 S32K144 Embedded Study

**S32K144EVB-Q100 보드를 기반으로 MCU 기본기 → 드라이버 실습 → 디버깅 → (향후) AUTOSAR 구조 이해까지 학습하는 저장소입니다.**
임베디드 초보자에서 실무 가능한 엔지니어로 성장하기 위한 실험 기록, 개념 정리, 예제 코드들을 포함합니다.

---

## 📂 Repository Structure

```
s32k144-embedded-study
│
├── README.md                 # 현재 문서
│
├── docs/                     # MCU 개념, 주변장치 정리
│   ├── MCU_basics.md
│   ├── S32K144_Overview.md
│   ├── Clock_System.md
│   ├── GPIO.md
│   ├── UART.md
│   ├── ADC.md
│   ├── Timer.md
│   ├── Interrupts.md
│   ├── Debugging.md
│   └── Build_Process.md
│
├── board_notes/              # 실습 일지 및 트러블슈팅 기록
│   ├── 2024-12-01-blinky.md
│   ├── 2024-12-05-uart-echo.md
│   └── …
│
├── examples/                 # MCU 실습 예제 코드
│   ├── gpio_blink/
│   │   ├── main.c
│   │   └── README.md
│   ├── uart_echo/
│   ├── adc_read/
│   └── timer_toggle/
│
├── s32ds_projects/           # S32 Design Studio 기반 전체 프로젝트
│   └── gpio_blink_project/
│
└── assets/                   # 블록도, 이미지, 핀맵 등
	├── s32k144_block_diagram.png
	├── board_pinout.jpg
	└── diagram.png
```

---

# 🎯 Study Goals

### ✔ 1. MCU 기본 구조 이해

레지스터, 메모리 맵, 클럭, IO 제어 등 **저수준 임베디드 기본기** 학습.

### ✔ 2. S32K144 주변장치 제어

GPIO / UART / Timer / ADC / PWM 등 실습.

### ✔ 3. 디버깅 능력 강화

* FreeMASTER
* S32DS Debugger
* 오실로스코프(가능한 경우)

### ✔ 4. SDK와 드라이버 구조 이해

* NXP S32 SDK 드라이버 구조
* Pins Tool, Clocks Tool 사용법

### ✔ 5. AUTOSAR 준비 (후반부)

현재 단계에서는 MCU 중심.
향후 다음 내용을 학습할 예정:

* AUTOSAR Layer 구조 (ASW/BSW)
* MCAL / BSW의 역할
* RTE의 동작 원리
* OS(Task / Event / Scheduling)

---

# 🧪 What’s Inside?

## 📘 docs/ – 개념 정리

MCU 이해도를 완성시키는 핵심 문서 모음.

예시:

* **MCU_basics.md**
  CPU, RAM, Flash, Bus, Peripherals 등 구조 설명
* **Clock_System.md**
  SIRC, FIRC, SOSC, SPLL 및 시스템 클럭 구성
* **GPIO.md**
  Port/Pin 구조 + 레지스터 기반 제어
* **Debugging.md**
  SWD, FreeMASTER, JTAG, Breakpoint 개념

---

## 📝 board_notes/ – 실험 기록

각 실습을 **목적 → 과정 → 문제 → 해결 → 다음 목표**로 정리.

예시:

```
# UART Echo 실험 (2024-12-05)
- 목적: LPUART 초기화 및 RX/TX 동작 확인
- 문제: Baudrate mismatch로 garbage data 발생
- 해결: Clock 설정 수정 및 oversampling 값 조정
- 다음 목표: Interrupt 기반 UART로 확장
```

---

## 💻 examples/ – MCU 실습 코드

직접 작성한 최소 예제 코드 모음.

예시:

### gpio_blink/main.c

* 포트 초기화
* LED 토글
* Delay loop
  (향후 → PIT 기반으로 개량)

### uart_echo/main.c

* LPUART 초기화
* RX → TX Loopback

---

## 🧰 s32ds_projects/

S32 Design Studio로 생성한 전체 프로젝트 폴더
(컴파일 가능 상태 유지)

회사 코드나 AUTOSAR 설정 파일은 포함하지 않음.

---

# 🚀 How I Study

1. **공식 Reference 문서를 읽고 정리**

   * S32K144 RM (Reference Manual)
   * EVB User Manual
   * SDK API Docs

2. **간단한 예제 먼저 작성**

   * GPIO → Timer → UART → ADC 순서

3. **문제 발생 시 ‘왜?’를 기준으로 디버깅**

   * Clock이 켜졌는가?
   * Pin mux는 맞는가?
   * Interrupt enable 됐는가?

4. **정리한 내용 GitHub에 업로드**

   * docs/ 에 개념 정리
   * examples/ 에 동작 코드
   * board_notes/ 에 실험 내역

---

# 📌 Future Work

* PWM / FTM 실습 추가
* Interrupt 기반 UART
* Simple Scheduler (Pre-AUTOSAR OS)
* FreeMASTER 변수 모니터링
* AUTOSAR Layer 구조 문서 정리
* MCAL 개념 학습

---

# 📎 References

* NXP S32K144EVB-Q100 Official Page
* S32 Design Studio for ARM
* S32K ARM MCU Reference Manual
* FreeMASTER User Guide
* AUTOSAR Specification (high-level)

---
