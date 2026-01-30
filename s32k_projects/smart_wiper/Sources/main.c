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

  volatile int exit_code = 0;

/* --- [Section 1] 상태 정의 및 전역 변수 --- */
typedef enum {
  MODE_OFF,
  MODE_INT,
  MODE_LOW,
  MODE_HIGH
} WiperMode_t;

WiperMode_t currentMode = MODE_OFF;
ftm_state_t ftmStateStruct;
uint16_t adcValue = 0;

/* --- [Section 2] 와이퍼 1회 왕복 함수 --- */
// moveTime: 0도에서 140도까지 가는데 걸리는 시간(ms)
void wipeOnce(uint16_t minDuty, uint16_t maxDuty, uint32_t moveTime)
{
    // 140도로 이동
    FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, maxDuty, 0U, true);
    OSIF_TimeDelay(moveTime);

    // 0도로 복귀
    FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, minDuty, 0U, true);
    OSIF_TimeDelay(moveTime);
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
	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
	ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);
	FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &ftmStateStruct);
	FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);

	// LPUART1 초기화 (image_b9299a.png 설정을 바탕으로 통로를 엽니다)
	LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);
	// FreeMASTER 드라이버 초기화
	FMSTR_Init(); // <--- 이 줄을 추가하세요

	/* [Section 3] 스마트 와이퍼 실행 루프 */
	for(;;) {
		// FreeMASTER 통신 처리 (가장 먼저 혹은 가장 나중에 배치)
		FMSTR_Poll();

		/* [STEP 1] ADC 읽기 (가변저항 값 획득) */
		ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);
		while(ADC_DRV_GetConvCompleteFlag(INST_ADCONV1, 0U) == false);
		ADC_DRV_GetChanResult(INST_ADCONV1, 0U, &adcValue);

		/* [STEP 2] ADC 값에 따른 상태 결정 (설계사양서 준수) */
		if (adcValue < 500)         currentMode = MODE_OFF;
		else if (adcValue < 2000)   currentMode = MODE_INT;
		else if (adcValue < 3500)   currentMode = MODE_LOW;
		else                        currentMode = MODE_HIGH;

		/* [STEP 3] 상태별 동작 수행 */
		switch(currentMode) {
			case MODE_OFF:
				// 0도 위치에서 대기
				FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, 800, 0U, true);
				break;

			case MODE_INT:
				// 1회 왕복 후 3초 대기
				wipeOnce(800, 3500, 500);
				OSIF_TimeDelay(3000);
				break;

			case MODE_LOW:
				// 느린 속도로 연속 왕복
				wipeOnce(800, 3500, 800);
				break;

			case MODE_HIGH:
				// 빠른 속도로 연속 왕복
				wipeOnce(800, 3500, 300);
				break;
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
