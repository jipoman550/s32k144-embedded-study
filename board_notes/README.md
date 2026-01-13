# S32K144 Embedded Study Notes

NXP S32K144EVB 보드를 활용한 임베디드 시스템 학습 기록입니다.
단순한 예제 실행을 넘어, **SDK 내부 동작 분석(Low-level)**과 **하드웨어 제어 원리(Register/Bitwise)** 이해를 목표로 합니다.

## 📚 Study Log

| Chapter | Project Name | Topic | Key Concepts | Date |
| :--- | :--- | :--- | :--- | :--- |
| [00](./00_gpio_output_led) | `00_gpio_output_led` | GPIO Output | GPIO, IDE Setup, Debugging | 2025.12.08 |
| [01](./01_freemaster_s32k144) | `01_freemaster_s32k144` | ADC & FreeMASTER | ADC(SAR), Register Masking, Oscilloscope | 2025.12.15 |
| [02](./02_RGB_Color_Mixer) | `02_RGB_Color_Mixer` | RGB Mixer | Interrupt, PWM, Color Mixing | 2025.12.20 |

---

## 📂 Project Details

### 🟢 00. GPIO Output LED
* **목표:** 보드 내장 LED(Red/Green/Blue) 제어를 통한 GPIO Output 동작 이해.
* **학습 내용:**
    * S32DS IDE 환경 구축 및 디버거(OpenSDA) 설정.
    * `PINS_DRV_WritePin` 등 SDK 함수 활용법 기초.

### 📈 01. FreeMASTER ADC Oscilloscope (Deep Dive)
* **목표:** 가변 저항(Potentiometer)의 전압 변화를 ADC로 읽어들이고, **FreeMASTER**를 통해 실시간 그래프(오실로스코프)로 시각화.

### 🎨 02. RGB Color Mixer (ADC + PWM + Interrupt)

* **목표:** 가변 저항의 아날로그 입력을 3개의 구간으로 수치화하고, 각 구간에 매칭되는 RGB LED의 밝기를 PWM으로 제어하여 실시간 색상 혼합(Color Mixing) 구현.
* **학습 내용:**
    * **하이브리드 제어 구조:** ADC 데이터 획득에는 **Polling** 방식을, FreeMASTER 통신 모니터링에는 **Interrupt** 방식을 적용하여 시스템 제어와 통신의 효율성 최적화.
    * **PWM Duty Cycle 제어:**  범위의 ADC 디지털 값을  범위의 PWM 듀티 사이클로 정밀 스케일링(Scaling)하는 수식 설계 및 구현.
    * **인터럽트 핸들러 수동 등록:** `INT_SYS_InstallHandler` 함수를 통해 LPUART1 하드웨어 인터럽트와 FreeMASTER 엔진(`FMSTR_Isr`)을 직접 연결하는 로우레벨(Low-level) 인터럽트 설정 실습.
    * **데이터 시각화:** **FreeMASTER**를 활용하여 가변 저항 회전에 따른 RGB 각 채널의 Duty Cycle 변화를 실시간 파형(Scope)으로 분석 및 검증.


---
