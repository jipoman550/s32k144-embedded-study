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
---
