# S32K144 Embedded Study Notes

NXP S32K144EVB 보드를 활용한 임베디드 시스템 학습 기록입니다.
단순한 예제 실행을 넘어, **SDK 내부 동작 분석(Low-level)**과 **하드웨어 제어 원리(Register/Bitwise)** 이해를 목표로 합니다.

## 📚 Study Log

| Chapter | Project Name | Topic | Key Concepts | Date |
| :--- | :--- | :--- | :--- | :--- |
| [00](./00_gpio_output_led) | `00_gpio_output_led` | GPIO Output | GPIO, IDE Setup, Debugging | 2025.12.08 |
| [01](./01_freemaster_s32k144) | `01_freemaster_s32k144` | ADC & FreeMASTER | ADC(SAR), Register Masking, Oscilloscope | 2025.12.15 |
| [02](./02_RGB_Color_Mixer) | `02_RGB_Color_Mixer` | RGB Mixer | Interrupt, PWM, Color Mixing | 2025.12.20 |
| [03](./03_smart_wiper) | `03_smart_wiper` | Basic Wiper Control | ADC, PWM, Polling | 2026.01.10 |
| [04](./04_smart_wiper_dma) | `04_smart_wiper_dma` | DMA Integration | eDMA, Memory-to-Memory | 2026.01.25 |
| [05](./05_smart_wiper_interrupt) | `05_smart_wiper_interrupt` | Timer & Interrupt | LPIT, ISR | 2026.02.10 |
| [06](./06_smart_wiper_refactored_dsp)| `06_smart_wiper_refactored_dsp`| Code Refactoring | MA Filter, Control Logic | 2026.02.25 |
| [07](./07_smart_wiper_CAN_v6) | `07_smart_wiper_CAN_v6` | CAN Loopback | CAN Tx/Rx, MISRA-C, Failsafe | 2026.03.15 |
| [08](./08_smart_wiper_CAN_2_v7)| `08_smart_wiper_CAN_2_v7`| Dual Node CAN | 2-Board CAN Communication | 2026.03.30 |

---

## 📂 Project Details

### 🟢 00. GPIO Output LED
* **목표:** 보드 내장 LED 제어를 통한 S32DS IDE 환경 구축 및 GPIO Output 기초 동작 이해.

### 📈 01. FreeMASTER ADC Oscilloscope (Deep Dive)
* **목표:** 가변 저항 전압 변화의 ADC 실시간 수집 및 FreeMASTER 오실로스코프 시각화.

### 🎨 02. RGB Color Mixer (ADC + PWM + Interrupt)
* **목표:** 하이브리드 제어(ADC Polling + FreeMASTER ISR)를 활용한 실시간 RGB 색상 혼합(PWM) 파형 분석 및 검증.

### 🚗 03. Smart Wiper (Basic)
* **목표:** 가변저항 ADC 값을 Servo Motor의 PWM Duty로 변환하는 기초 와이퍼 제어 구현.

### ⚡ 04. Smart Wiper with DMA
* **목표:** CPU 개입 없이 ADC 데이터를 메모리로 직접 이동시키는 eDMA 기술 적용.

### ⏱️ 05. Smart Wiper with Interrupt
* **목표:** LPIT 타이머를 활용하여 10ms 주기의 정밀 반복 제어(ISR) 시스템 구축.

### 🛠️ 06. Smart Wiper Refactoring & DSP
* **목표:** 이동 평균 필터(Moving Average) 적용 및 하이브리드 제어 로직 리팩토링으로 신호 안정성 확보.

### ⚙️ 07. Smart Wiper CAN Loopback (v6)
* **목표:** 1대 보드 내 FlexCAN 루프백 통신, Failsafe 로직 강화, ISR 다이어트 및 MISRA-C 검증 도입.

### 🔗 08. Smart Wiper Dual Node CAN (v7)
* **목표:** 2대의 S32K144 보드를 물리적 CAN 버스로 연결하여 상호 데이터 송수신 네트워크 구축.

---
