# 01. GPIO Output - LED Blinking

## 1. 학습 목표
* S32K144EVB 보드의 RGB LED(Blue/Red)를 제어한다.
* **S32 Design Studio (S32DS)의 프로젝트 관리 및 디버깅 환경을 구축한다.** (가장 중요)

## 2. 하드웨어 분석
* **MCU:** NXP S32K144
* **LED 연결 (Schematic):**
    * BLUE LED: `PTD0` (Port D, 0번 핀)
    * RED LED: `PTD15` (Port D, 15번 핀)
    * **동작 방식:** Output Low(0) -> ON / Output High(1) -> OFF (회로도 확인 필요)

## 3. 개발 환경 구축 (Troubleshooting Log)
**이 프로젝트를 시작하면서 겪었던 환경 설정 이슈와 해결책을 정리함.**

### 3.1. 프로젝트 위치 선정 (Windows vs WSL)
* **이슈:** 코드는 WSL(Ubuntu)에서 `git`으로 관리하고 싶고, 빌드는 Windows S32DS에서 해야 함.
* **해결:**
    1. Git 리포지토리를 **Windows 파일 시스템(`C:\Embedded_Study\...`)**에 생성.
    2. WSL2 터미널에서 `/mnt/c/Embedded_Study/...`로 접근하여 git 관리.
    3. S32DS는 Windows 경로의 파일을 직접 로드.
    * *교훈: WSL 내부에 프로젝트를 두면 S32DS가 경로를 못 찾거나 빌드가 매우 느려짐.*

### 3.2. 기존 프로젝트 불러오기 (Import)
* **상황:** Git으로 클론한 프로젝트를 S32DS에 인식시켜야 함.
* **방법:**
    1. `File` -> `Import` -> `General` -> `Existing Projects into Workspace`
    2. **주의:** `Copy projects into workspace` 체크박스를 **반드시 해제**해야 함. (체크하면 파일이 복사되어 Git 관리가 꼬임)

![Import 설정 화면](./images/import_project_1.png)
![Import 설정 화면](./images/import_project_2.png)

### 3.3. 실행 에러 (Run vs Debug)
* **에러:** 상단 재생 버튼(Run)을 눌렀더니 `Cannot run program... launching failed` 에러 발생.
![Run 실행 에러](./images/run_error.png)

* **원인:** PC(x86)에서 ARM용 `.elf` 파일을 실행하려 했기 때문.
* **해결:**
    1. **Run(재생 버튼) 금지.**
    2. 벌레 모양 아이콘(`Debug`) 옆 화살표 -> `Debug Configurations`
    3. `GDB PEMicro Interface Debugging` 선택.
    4. Interface가 `OpenSDA` (USB)인지 확인 후 `Debug` 클릭.
![디버그 설정 화면](./images/debug_config.png)

    5. 이후에는 **F8 (Resume)** 키를 눌러서 실행.
![Perspective 전환 화면](./images/perspective.png)
## 4. 코드 구현 (main.c)

### 주요 함수
```c
// 하드웨어 초기화 (pin_mux.c 설정 적용)
PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

// 핀 출력 토글
// PTD: Port D의 주소
// 1U << 15: 15번 비트(핀)를 제어
PINS_DRV_TogglePins(PTD, 1U << 15);