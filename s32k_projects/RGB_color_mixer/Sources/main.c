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
#include "Cpu.h"

/* [수정 1] 필요한 헤더 파일들을 여기에 추가합니다. */
#include "clockMan1.h"      /* 클럭 설정 정보 */
#include "pin_mux.h"        /* 핀 설정 정보 (NUM_OF_CONFIGURED_PINS0 등) */
#include "flexTimer_pwm1.h" /* FTM 설정 정보 (INST_FLEXTIMER_PWM1 등) */
#include "ftm_pwm_driver.h" /* FTM 드라이버 함수 (FTM_DRV_UpdatePwmChannel 등) */
#include "osif.h"           /* 시간 지연 함수 (OSIF_TimeDelay) */

/* [추가] ADC 드라이버 사용을 위한 헤더 파일 */
#include "adConv1.h"

ftm_state_t flexTimer_pwm1_State;

  volatile int exit_code = 0;

/* User includes (#include below this line is not maintained by Processor Expert) */

/* 아무 기능 없이 CPU만 뱅뱅 돌려서 시간 끄는 함수 */
void delay_dummy(volatile int cycles) {
  while(cycles--) {
	  __asm("nop"); /* No Operation (아무것도 안 함) */
  }
}


/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
  /* Write your local variable definition here */
    /* 변수 선언 */
    uint16_t adcValue; /* ADC로부터 읽어온 디지털 값 (0~4095) */
    uint16_t duty;     /* PWM에 적용할 듀티 값 (0~10000) */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
    /* 1. 클럭 매니저 초기화 */
    CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
    CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);

    /* 2. 핀 초기화 (PTD15, 16, 0 및 ADC 입력 핀 설정) */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

    /* 3. FTM 초기화 및 PWM 설정 */
    FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &flexTimer_pwm1_State);
    FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);

    /* adConv1.h의 리스트에 따라 ConfigConverter로 초기화 진행 */
    ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);

    for(;;) {
        /* 1. 채널 설정 및 변환 시작 */
        ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);

        /* 2. 변환 완료 대기 (헤더 54행의 GetConvCompleteFlag 사용) */
        while(ADC_DRV_GetConvCompleteFlag(INST_ADCONV1, 0U) == false);

        /* 3. 결과 읽기 (헤더 55행에 따라 3개의 인자 사용 및 주소 전달) */
        ADC_DRV_GetChanResult(INST_ADCONV1, 0U, &adcValue);

        /* 4. PWM 업데이트 로직 */
        duty = (uint16_t)((uint32_t)adcValue * 10000 / 4095);
        FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_TICKS, duty, 0U, true);

        delay_dummy(100000);

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
