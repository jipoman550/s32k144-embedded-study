# 02. RGB Color Mixer (ADC + PWM + Interrupt)

## 🎯 학습 목표
ADC로 가변 저항의 전압을 읽고, 그 값을 기반으로 PWM 신호를 생성하여 RGB LED의 색상과 밝기를 실시간으로 제어합니다. 또한, 이 과정을 FreeMASTER로 모니터링합니다.

---

## 🧠 1교시: 핵심 개념 잡기

### 1. 인터럽트 (Interrupt)
CPU가 메인 루프(`while(1)`)를 돌고 있을 때, 특정 이벤트(ADC 완료, 타이머 종료 등)가 발생하면 **하던 일을 멈추고 즉시 전용 처리 함수(ISR)**를 실행하는 방식입니다.

* **비유:** * **폴링(Polling):** 배달이 왔는지 확인하려고 1분마다 현관문을 열어보는 것 (CPU 낭비 심함)
    * **인터럽트(Interrupt):** 배달이 오면 '초인종(Interrupt)'이 울릴 때까지 내 일을 하는 것 (효율적)
* **하드웨어 연결:** S32K144의 ADC 모듈은 변환이 완료되면 CPU에 인터럽트 신호를 보낼 수 있습니다.

### 2. PWM (Pulse Width Modulation)
디지털 신호의 **'켜짐(High)'과 '꺼짐(Low)' 시간 비율**을 조절하여, LED에 전달되는 평균 전력을 제어하는 기술입니다. 전압을 직접 낮추지 않고도 밝기를 조절할 수 있습니다.

* **Duty Cycle (듀티 사이클):** 한 주기 동안 신호가 High인 비율입니다.
  $$Duty\ Cycle = \frac{T_{on}}{T_{period}} \times 100\%$$
* **RGB Mixing:** 빨강(R), 초록(G), 파랑(B) LED 각각의 PWM Duty Cycle을 다르게 주면 수만 가지의 색상을 조합할 수 있습니다.

---

## 🛠️ 하드웨어 구성
* **입력:** Potentiometer (ADC0_SE12) -> 전압을 0~4095 숫자로 변환.
* **출력:** RGB LED (FTM0 PWM 채널 활용)
* **모니터링:** FreeMASTER (LPUART1 활용)