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
	/* [처방 1] 시작하자마자 불이 켜져야 합니다! (50% 밝기) */
	uint16_t duty = 5000;
	bool increasing = true;

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
	/* 1. 클럭 매니저 초기화: 칩의 모든 모듈에 심장박동(Clock)을 공급합니다. */
	CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
	CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);

	/* 2. 핀 초기화: 우리가 설정한 PTD15, 16, 0 핀을 FTM 모드로 전환합니다. */
	status_t st = PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
	if (st != STATUS_SUCCESS) { for (;;){} }
	FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &flexTimer_pwm1_State);
	/* 3. PWM 초기화: FTM0 모듈을 우리가 설정한 주기(10000)로 가동합니다. */
	st = FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);
	if (st != STATUS_SUCCESS) { for (;;){} }
  for(;;) {
	/* 빨간색 LED(Channel 0)의 밝기를 현재 duty 값으로 업데이트합니다. */
	FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_TICKS, duty, 0U, true);

    /* 밝기 조절 로직 (언더플로우 방지 버전) */
    if (increasing) {
        duty += 100;
        if (duty >= 10000) {
            duty = 10000;
            increasing = false;
        }
    } else {
        /* 여기서 0보다 작아지는 것을 미리 막아야 합니다! */
        if (duty <= 100) {
            duty = 0;
            increasing = true;
        } else {
            duty -= 100;
        }
    }

	/* 너무 빠르면 눈이 인식 못 하므로 10ms 정도 기다려줍니다. (OSIF는 SDK 기본 제공 지연 함수) */
	//OSIF_TimeDelay(10);

    /* [처방 2] 지연 시간을 짧게 줄임 */
    delay_dummy(500000);

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
