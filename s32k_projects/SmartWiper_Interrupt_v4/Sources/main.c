/*
 * [FILE] main.c
 * [ROLE] 스마트 와이퍼 시스템의 메인 제어 모듈
 * [VERSION] v4 (LPIT Interrupt & DMA Integration)
 * [AUTHOR] S32K144 Embedded Mentor
 */

/* ---------------------------------------------------------------- 프로젝트
 * 헤더 포함 */
#include "Cpu.h"
#include "adConv1.h"
#include "clockMan1.h"
#include "dmaController1.h"
#include "flexTimer_pwm1.h"
#include "freemaster.h"
#include "lpit1.h"
#include "osif.h"
#include "pin_mux.h"


volatile int exit_code = 0;

/* ---------------------------------------------------------------- 전역 상수 및
 * 정의 */
#define POS_0_DEG (800U)      /* 서보모터 0도 위치 (PWM Duty) */
#define POS_140_DEG (3300U)   /* 서보모터 140도 위치 (PWM Duty) */
#define INT_WAIT_TIME (3000U) /* 간헐적 모드(INT) 대기 시간 (3000ms) */

/* ---------------------------------------------------------------- 사용자 정의
 * 자료형 */
typedef enum {
  MODE_OFF = 0U,
  MODE_INT = 1U,
  MODE_LOW = 2U,
  MODE_HIGH = 3U
} WiperMode_t;

typedef enum {
  WIPER_IDLE = 0U,
  WIPER_MOVING_UP = 1U,
  WIPER_MOVING_DOWN = 2U
} WiperStep_t;

/* ---------------------------------------------------------------- 전역 변수
 * 설정 */
static volatile WiperMode_t currentMode = MODE_OFF;
static volatile WiperStep_t currentStep = WIPER_IDLE;
static ftm_state_t ftmStateStruct;

/* DMA(배달부)가 ADC0 결과를 실시간으로 복사해올 목적지 주소 */
volatile uint16_t adcValue = 0U;

/* 시스템 모니터링용 변수 */
volatile uint32_t ms_ticks = 0U;
volatile uint32_t loop_cnt = 0U;

/* FreeMASTER 관찰을 위해 전역으로 뺀 카운터 */
// volatile uint32_t stepCounter = 0U;

/* 통신(FreeMASTER) 제어용 변수 */
volatile uint8_t controlSource = 0U; /* 0: 가변저항, 1: PC 제어 */
volatile WiperMode_t pcModeRequest = MODE_OFF;

/* ---------------------------------------------------------------- 인터럽트
 * 서비스 루틴 */
void LPIT0_Ch0_IRQHandler(void) {
  /* [ROLE 1] 인터럽트 플래그 클리어 */
  LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1U << 0U));

  ms_ticks += 10U;

  /* [ROLE 2] 입력 데이터 기반 모드 결정 (Input Logic) */
  if (controlSource == 0U) {
    if (adcValue < 500U) {
      currentMode = MODE_OFF;
    } else if (adcValue < 2000U) {
      currentMode = MODE_INT;
    } else if (adcValue < 3500U) {
      currentMode = MODE_LOW;
    } else {
      currentMode = MODE_HIGH;
    }
  } else {
    currentMode = pcModeRequest;
  }

  /* [ROLE 3] 와이퍼 구동 상태 머신 (Control Logic) */
  static uint32_t stepCounter = 0U;
  uint32_t moveDuration = 0U;

  if (currentMode == MODE_OFF) {
    (void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
                                   FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U,
                                   true);
    currentStep = WIPER_IDLE;
    stepCounter = 0U;
  } else {
    if (currentMode == MODE_INT) {
      moveDuration = 500U;
    } else if (currentMode == MODE_LOW) {
      moveDuration = 800U;
    } else {
      moveDuration = 300U;
    }

    if (currentStep == WIPER_IDLE) {
      currentStep = WIPER_MOVING_UP;
      stepCounter = 0U;
      (void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
                                     FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_140_DEG,
                                     0U, true);
    }

    stepCounter +=
        10U; // 얘는 왜 여기에 있어야 하는거지? 다른 곳에 있으면 안되냐?

    if (currentStep == WIPER_MOVING_UP) {
      if (stepCounter >= moveDuration) {
        currentStep = WIPER_MOVING_DOWN;
        stepCounter = 0U;
        (void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
                                       FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG,
                                       0U, true);
      }
    } else if (currentStep == WIPER_MOVING_DOWN) {
      uint32_t waitTarget = (currentMode == MODE_INT)
                                ? (moveDuration + INT_WAIT_TIME)
                                : moveDuration;

      if (stepCounter >= waitTarget) {
        currentStep = WIPER_IDLE;
        stepCounter = 0U;
      }
    } else {
      currentStep = WIPER_IDLE;
    }
  }
}

/* User includes (#include below this line is not maintained by Processor
 * Expert) */

/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void) {
/* Write your local variable definition here */

/*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
#ifdef PEX_RTOS_INIT
  PEX_RTOS_INIT(); /* Initialization of the selected RTOS. Macro is defined by
                      the RTOS component. */
#endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* [STEP 2] 시스템 클럭 및 핀 초기화 */
  (void)CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT,
                       g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
  (void)CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
  (void)PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

  /* [STEP 3] eDMA 엔진 가동 */
  (void)EDMA_DRV_Init(&dmaController1_State, &dmaController1_InitConfig0,
                      edmaChnStateArray, edmaChnConfigArray, 1U);

  /* [STEP 4] ADC 및 DMA 데이터 배달 경로 설정 */
  ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);
  (void)EDMA_DRV_ConfigSingleBlockTransfer(
      0U, EDMA_TRANSFER_PERIPH2PERIPH, (uint32_t)&(ADC0->R[0]),
      (uint32_t)&adcValue, EDMA_TRANSFER_SIZE_2B, 2U);
  (void)EDMA_DRV_StartChannel(0U);
  ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);

  /* [STEP 5] 타이머(PWM/LPIT) 및 통신 초기화 */
  (void)FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig,
                     &ftmStateStruct);
  (void)FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);

  (void)LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
  (void)LPIT_DRV_InitChannel(INST_LPIT1, 0, &lpit1_ChnConfig0);
  LPIT_DRV_StartTimerChannels(INST_LPIT1, (1U << 0U)); /* 10ms 타이머 시작 */

  /* [STEP 6] 통신 및 글로벌 인터럽트 활성화 */
  (void)LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);
  (void)INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, (isr_t *)0);
  INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);

  FMSTR_Init();
  INT_SYS_EnableIRQGlobal(); /* 전역 인터럽트 허용: 이제부터 ISR 작동 시작 */

  /* [STEP 7] 백그라운드 무한 루프 */
  for (;;) {
    FMSTR_Poll();
    loop_cnt++;

    if (exit_code != 0) {
      break;
    }
  }

/*** Don't write any code pass this line, or it will be deleted during code
 * generation. ***/
/*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component.
 * DON'T MODIFY THIS CODE!!! ***/
#ifdef PEX_RTOS_START
  PEX_RTOS_START(); /* Startup of the selected RTOS. Macro is defined by the
                       RTOS component. */
#endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for (;;) {
    if (exit_code != 0) {
      break;
    }
  }
  return exit_code;
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
