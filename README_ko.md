# 📘 S32K144 임베디드 시스템: 스마트 와이퍼 아키텍처의 진화
> **42경산(소프트웨어 기본기)에서 시작하여 차량용 시스템 아키텍처까지.**

[English](README.md) | [한국어](README_ko.md)

본 레포지토리는 NXP S32K144EVB를 활용한 스마트 와이퍼 시스템의 개발 과정을 기록한 엔지니어링 저널입니다. 단순한 기능 구현에 그치지 않고, 하드웨어 가속(eDMA), 디지털 신호 처리(DSP), 그리고 **차량용 표준 네트워크(CAN)** 및 **AUTOSAR 아키텍처**로 진화해 나가는 아키텍처적 여정을 담고 있습니다.

---

## 🏎️ 프로젝트 로드맵: 아키텍처의 진화 과정

차량용 전자제어장치(ECU)에서 가장 중요한 **실시간성(Real-time processing)**과 **절대적인 신뢰성(Reliability)**을 확보하기 위한 아키텍처적 패러다임 변화에 초점을 맞추었습니다.

### [Phase 1] 기반 다지기 (v1 ~ v4)
- **FSM 제어**: 유한 상태 머신(FSM) 기반의 와이퍼 모드 제어(STOP, INT, LOW, HIGH) 구현.
- **하드 실시간성**: LPIT 타이머 인터럽트를 통해 엄격한 10ms 주기 제어 보장.
- **하드웨어 가속**: ADC 데이터 수집 부하를 eDMA로 이관하여 CPU 오버헤드가 없는 "Zero-overhead" 센싱 달성.

### [Phase 2] 신뢰성 확보 및 DSP (v5)
- **신호 무결성**: 센서 노이즈를 제거하고 제어 안정성을 확보하기 위해 **이동 평균 필터(Moving Average Filter)** 도입.
- **Fail-safe 설계**: 하드웨어 고장(오픈/쇼트) 및 급격한 신호 이상 변동을 감지하는 진단 로직(Delta Check) 구현.
- **계층형 리팩토링**: 최상위 애플리케이션 로직과 하드웨어 추상화 계층(HAL)을 엄격히 분리하여 코드 재사용성 극대화.

### [Phase 3] 분산 시스템 및 네트워크 (v6 ~ v7)
- **비동기 최적화 (v6)**: 무거운 연산 로직을 ISR에서 메인 루프로 이관하는 **Foreground-Background 스케줄링** 도입.
  - *도입 이유*: 향후 비동기 CAN 통신 및 대규모 데이터 처리에 대비하여 인터럽트 지연 시간(Latency)을 최소화하기 위함.
- **차량용 네트워크 (v7)**: 물리 CAN 버스를 구축하여 Master-Slave 구조의 분산 제어 환경 구현.
- **네트워크 Fail-safe**: CAN Bus-off 또는 통신 유실(Timeout) 감지 시, Slave 노드가 스스로 안전 상태(Wiper Home Position)로 복귀하도록 설계.

### [Phase 4] 표준화 및 고도화 (현재 진행형) 🚀
- **AUTOSAR 패러다임**: NXP RTD 및 **EB tresos** 에코시스템을 활용한 플랫폼 표준화 진행.
- **정밀 제어**: DC 모터 및 엔코더 피드백 제어를 위한 PID 제어기 구현.

---

## 🔌 기술적 깊이 (Technical Deep Dive)

### 1. 전경-배경 스케줄링 및 CPU 부하 분석 (v6)
인터럽트 서비스 루틴(ISR) 내부의 무거운 제어 연산을 `while(1)` 백그라운드 루프로 이관하여 시스템 응답성을 극대화했습니다. 10ms ISR은 이제 최소한의 이벤트 시그널링(Flag)만 담당합니다. 이 구조적 개선을 통해 확보된 CPU의 여유 대역폭(Slack Time)은 10초간 메인 루프가 회전하는 횟수(`loop_cnt`)를 기준으로 정량적으로 검증했습니다.

**하드웨어 최적화 정량적 지표 (10초 기준 루프 카운트 및 부하 분석):**

| 측정 시나리오 | 메인 루프 회전 수 (Loop Count) | CPU 사용률 (Usage) | CPU 여유 대역폭 (Slack Time) | 엔지니어링 신뢰성 판정 |
| :--- | :---: | :---: | :---: | :--- |
| **Baseline** (No Task) | 12.7M 회 | 0% | **100%** | 기준 유휴(Idle) 상태 |
| **Active** (Wiper Task On) | 4.7M 회 | 약 63% | **37%** | **정상권** (차량용 시스템 제어 안정 margin 70% 이하 충족) |

<details>
<summary>📊 FreeMASTER 실시간 데이터 그래프 보기</summary>

#### Baseline (No Task) - 12.7M 회
*<img src="board_notes/07_smart_wiper_CAN_v6/loop_cnt_without_ISR.png" alt="Loop Count Baseline" width="600"/>*

#### Active (Wiper Task On) - 4.7M 회
*<img src="board_notes/07_smart_wiper_CAN_v6/loop_cnt_with_ISR.png" alt="Loop Count Active" width="600"/>*

*(FreeMASTER DAQ를 통해 10초 동안 메인 루프 카운트를 샘플링하여 아키텍처 변경 후의 실시간 부하 및 여유 대역폭을 데이터로 증명한 결과입니다.)*
</details>

### 2. 저오버헤드 DSP 파이프라인
eDMA가 CPU 개입 없이 ADC 샘플을 메모리로 자동 전송하면, CPU는 정해진 주기 안에서 필터링만 수행합니다. Cortex-M4F 코어의 연산 효율을 극대화하기 위해, 메인 루프 내 평균 연산 시 나눗셈 대신 레지스터 레벨의 비트 우측 시프트 연산(`adcSum >> 3U`)을 사용하여 연산 클럭을 극도로 압축했습니다.

### 3. 차량용 고장 안전 조치 (Fail-safe Strategy)
안전이 최우선인 차량용 소프트웨어를 위해, 센서 변화량이 물리적 한계(`MAX_DELTA`)를 초과하거나 전기적 고장이 진단되면 제어기는 즉시 고장 안전 조치(Safe-state)를 발동합니다. 이때 시스템은 사용자 입력을 무시하고 `MODE_OFF`로 강제 고정되며 와이퍼를 즉시 홈 포지션으로 복귀시킵니다.

---

## 🛠 기술 스택 및 사용 도구

- **MCU**: NXP S32K144 (ARM Cortex-M4F)
- **IDE / Tools**: S32 Design Studio v3.5, **EB tresos Studio**, FreeMASTER DAQ
- **Protocols**: CAN 2.0B, LPUART, SPI
- **Architecture**: Foreground-Background, AUTOSAR Classic (In Progress)
- **Language**: Embedded C (MISRA-C 컴플라이언스 지향)

---

## 📝 엔지니어링 철학 (Engineering Practices)

- **레퍼런스 매뉴얼 기반의 Bare-Metal 제어**: generic 튜토리얼이나 블랙박스 SDK에 의존하지 않고, **S32K1xx Reference Manual**과 데이터시트를 직접 분석하여 레지스터 레벨(MMIO)에서 페리페럴을 제어했습니다.
- **데이터 기반의 검증**: 제어 루프 튜닝 및 성능 검증 시 감에 의존하지 않고, **FreeMASTER DAQ 그래프**를 통해 메모리 변수를 실시간 시각화하여 정량적으로 검증했습니다.
- **시스템 관점의 사고**: 개별 페리페럴의 독립적인 동작보다, 시스템 전체의 데이터 흐름, 태스크 우선순위, 실시간성 제약 조건(Slack Time)의 균형을 우선시합니다.

---

## 📎 참고 자료
- [S32K1xx Series Reference Manual](https://www.nxp.com/webapp/Download?colCode=S32K1XXRM)
- [AN5413: S32K144 Series Cookbook](https://www.nxp.com/webapp/Download?colCode=AN5413)
