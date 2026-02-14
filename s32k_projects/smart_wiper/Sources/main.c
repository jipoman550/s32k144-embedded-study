/* ###################################################################
**     Filename    : main.c
**     Processor   : S32K1xx
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 01.00
** @brief
**         Main module.
**         This module contains user's application code.
*/
/*!
**  @addtogroup main_module main module documentation
**  @{
*/
/* MODULE main */


/* Including necessary module. Cpu.h contains other modules needed for compiling.*/
/* 1. 필요한 드라이버 헤더 파일들을 include 섹션에 추가합니다. */
#include "Cpu.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "adConv1.h"
#include "flexTimer_pwm1.h"
#include "osif.h"
#include "freemaster.h"
//#include "watchdog1.h"

  volatile int exit_code = 0;

/* --- [Section 1] 상태 정의 및 전역 변수 --- */
typedef enum {
  MODE_OFF,
  MODE_INT,
  MODE_LOW,
  MODE_HIGH
} WiperMode_t;

// 와이퍼의 세부 동작 상태
typedef enum {
    WIPER_IDLE,
    WIPER_MOVING_UP,   // 0 -> 140도로 이동 중
    WIPER_MOVING_DOWN  // 140 -> 0도로 복귀 중
} WiperStep_t;

WiperMode_t currentMode = MODE_OFF;
WiperStep_t currentStep = WIPER_IDLE;
ftm_state_t ftmStateStruct;
uint16_t adcValue = 0;
uint32_t currentTime = 0;
uint32_t lastTime = 0;
volatile uint32_t ms_ticks = 0; // 1ms마다 1씩 증가할 진짜 시계

/* [추가] 제어권 및 PC 명령 변수 */
volatile uint8_t controlSource = 0;    // 0: 가변저항(ADC), 1: PC(FreeMASTER)
volatile WiperMode_t pcModeRequest = MODE_OFF; // PC에서 보낼 모드 명령


/* --- [Section 2] 설정값 --- */
#define POS_0_DEG      800
#define POS_140_DEG    3300
#define INT_WAIT_TIME  3000 // 3초 대기

/* --- [Section 2] LPIT 인터럽트 서비스 루틴 (ISR) --- */
// 1ms마다 하드웨어가 이 함수를 자동으로 호출합니다.
void LPIT0_Ch0_IRQHandler(void)
{
    // 인터럽트 플래그를 지워줘야 다음 인터럽트가 발생합니다.
    LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1 << 0));
    ms_ticks++; // 시계 숫자 증가
}


/* User includes (#include below this line is not maintained by Processor Expert) */

/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
    /* 하드웨어 초기화 (기존 코드 유지) */
	CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
	CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);

	/* 2. 워치독 강제 비활성화 (Nuclear Option) */
	// 워치독 레지스터에 직접 접근하여 잠금을 해제하고 EN 비트를 끕니다.
//	WDOG->CNT = 0xD928C520;              // Unlock 키 1
//	WDOG->CNT = 0xD928C520;              // Unlock 키 2
//	while((WDOG->CS & (1U << 11)) == 0); // 잠금 해제될 때까지 대기
//	WDOG->CS &= ~(1U << 7);              // EN(Enable) 비트 해제 (완전히 끄기)

	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
	ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);
	FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &ftmStateStruct);
	FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);

	// LPIT 타이머 초기화 및 시작
	LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
	LPIT_DRV_InitChannel(INST_LPIT1, 0, &lpit1_ChnConfig0);
	LPIT_DRV_StartTimerChannels(INST_LPIT1, (1 << 0));

	// 1. LPUART1 물리 계층 초기화
	LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);

	// 2. LPUART 인터럽트와 FreeMASTER 서비스 루틴 연결
	// UART로 데이터가 들어오면 CPU가 FMSTR_Isr 함수로 바로 점프하게 만듭니다.
	INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, NULL);

	// 3. 하드웨어 레벨에서 LPUART 인터럽트 통로 개방
	INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);

	// 4. FreeMASTER 드라이버 상위 계층 초기화
	FMSTR_Init();

	// 5. 시스템 전체 인터럽트 활성화 (LPIT와 LPUART 모두를 위해 필수)
	INT_SYS_EnableIRQGlobal();

    // 비차단 로직을 위한 시간 관리 변수
    uint32_t moveDuration = 500; // 기본 이동 시간

	/* [Section 3] 스마트 와이퍼 실행 루프 */
	for(;;) {
		// FreeMASTER 통신 처리 (가장 먼저 혹은 가장 나중에 배치)
		FMSTR_Poll();

		currentTime = ms_ticks;

		/* [STEP 1] ADC 읽기 (가변저항 값 획득) */
		ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);
		while(ADC_DRV_GetConvCompleteFlag(INST_ADCONV1, 0U) == false);
		ADC_DRV_GetChanResult(INST_ADCONV1, 0U, &adcValue);

		/* [STEP 2] ADC 값에 따른 상태 결정 (설계사양서 준수) */
		if (controlSource == 0) {
			/* [기존 로직] 가변저항(ADC) 대장 모드 */
			if (adcValue < 500)       currentMode = MODE_OFF;
			else if (adcValue < 2000) currentMode = MODE_INT;
			else if (adcValue < 3500) currentMode = MODE_LOW;
			else                      currentMode = MODE_HIGH;
		}
		else {
			/* [새로운 로직] PC(FreeMASTER) 대장 모드 */
			// PC에서 pcModeRequest 값을 바꾸면 즉시 와이퍼 모드가 바뀝니다.
			currentMode = pcModeRequest;
		}

		/* [STEP 3] 비차단 상태별 동작 수행 */
		switch(currentMode) {
			case MODE_OFF:
				FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
				currentStep = WIPER_IDLE;
				break;

			case MODE_INT:    moveDuration = 500; break;
			case MODE_LOW:    moveDuration = 800; break;
			case MODE_HIGH:   moveDuration = 300; break;
		}

		// 와이퍼 왕복 로직 (MODE_OFF가 아닐 때만 작동)
		if (currentMode != MODE_OFF) {
			// 1. 대기 상태 -> 위로 이동 시작
			if (currentStep == WIPER_IDLE) {
				currentStep = WIPER_MOVING_UP;
				lastTime = currentTime;
				FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_140_DEG, 0U, true);
			}
			// 2. 위로 이동 완료 체크 -> 아래로 이동 시작
			else if (currentStep == WIPER_MOVING_UP) {
				if (currentTime - lastTime >= moveDuration) {
					currentStep = WIPER_MOVING_DOWN;
					lastTime = currentTime;
					FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
				}
			}
			// 3. 아래로 이동 완료 체크 -> IDLE로 복귀 (대기 시간 포함)
			else if (currentStep == WIPER_MOVING_DOWN) {
				uint32_t waitTarget = (currentMode == MODE_INT) ? (moveDuration + INT_WAIT_TIME) : moveDuration;

				if (currentTime - lastTime >= waitTarget) {
					currentStep = WIPER_IDLE; // 다시 처음부터 시작할 준비 완료
				}
			}
		}

        if(exit_code != 0) {
          break;
        }
      }


  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;) {
    if(exit_code != 0) {
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
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.1 [05.21]
**     for the NXP S32K series of microcontrollers.
**
** ###################################################################
*/
