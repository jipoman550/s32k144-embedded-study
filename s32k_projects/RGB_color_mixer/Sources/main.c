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
/* 필요한 드라이버 헤더 파일들 */
#include "Cpu.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "flexTimer_pwm1.h"
#include "ftm_pwm_driver.h"
#include "osif.h"
#include "adConv1.h"

/* [추가] UART 및 FreeMASTER 헤더 파일 */
#include "lpuart1.h"      /* UART 통신 채널 설정을 위함 */
#include "freemaster.h"   /* FreeMASTER 프로토콜 엔진 사용을 위함 */

/* [중요] FreeMASTER에서 관찰할 변수들은 반드시 전역(Global) 변수로 선언해야 합니다. */
uint16_t adcValue;        /* 가변 저항 읽기값 (0~4095) */
uint16_t dutyRed;         /* 빨간색 LED 출력값 (0~10000) */
uint16_t dutyGreen;       /* 녹색 LED 출력값 (0~10000) */
uint16_t dutyBlue;        /* 파란색 LED 출력값 (0~10000) */

/* 드라이버 상태 저장용 구조체 (메모장 역할) */
ftm_state_t flexTimer_pwm1_State;    /* FTM 상태 저장 */

  volatile int exit_code = 0;

/* User includes (#include below this line is not maintained by Processor Expert) */

/* 아무 기능 없이 CPU만 뱅뱅 돌려서 시간 끄는 함수 */
//void delay_dummy(volatile int cycles) {
//  while(cycles--) {
//	  __asm("nop"); /* No Operation (아무것도 안 함) */
//  }
//}


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
    //uint16_t adcValue; /* ADC로부터 읽어온 디지털 값 (0~4095) */
    //uint16_t duty;     /* PWM에 적용할 듀티 값 (0~10000) */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
	/* 1. 클럭 매니저 초기화: 모든 하드웨어에 전기(심장박동) 공급 */
	CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
	CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);

	/* 2. 핀 초기화: PTD15(R), PTD16(G), PTD0(B) 및 PTC6/7(UART) 통로 개방 */
	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

	/* 3. UART 초기화: PC와 대화할 시리얼 통로를 엽니다 (LPUART1) */
	LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);

	/* [핵심 수정] 예제처럼 FreeMASTER 인터럽트 핸들러를 직접 등록합니다. */
	/* 이 코드가 들어가면 이전에 넣었던 DisableIRQ 코드는 지워주세요. */
	INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, (isr_t*)0);
	INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);

	/* 4. FreeMASTER 엔진 초기화: 통신 프로토콜을 준비시킵니다 */
	FMSTR_Init();

	/* 5. FTM(PWM) 초기화: LED 밝기를 조절할 엔진 가동 */
	FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &flexTimer_pwm1_State);
	FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);

	/* 6. ADC 초기화: 가변 저항 전압을 읽을 준비 */
	ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);

	for(;;) {
		/* [동작 1] 가변 저항 값 읽기 (0~4095) */
		ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);
		while(ADC_DRV_GetConvCompleteFlag(INST_ADCONV1, 0U) == false);
		ADC_DRV_GetChanResult(INST_ADCONV1, 0U, &adcValue);

		/* [동작 2] 하나의 가변 저항 값으로 3개의 색상 밝기 계산 (구간 분할) */
		if (adcValue < 1365) {
			dutyRed = (uint16_t)((uint32_t)adcValue * 10000 / 1365);
			dutyGreen = 0; dutyBlue = 0;
		} else if (adcValue < 2730) {
			dutyRed = 0;
			dutyGreen = (uint16_t)((uint32_t)(adcValue - 1365) * 10000 / 1365);
			dutyBlue = 0;
		} else {
			dutyRed = 0; dutyGreen = 0;
			dutyBlue = (uint16_t)((uint32_t)(adcValue - 2730) * 10000 / 1365);
		}

		/* [동작 3] 실제 LED 밝기에 반영 */
		FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_TICKS, dutyRed, 0U, true);
		FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 1U, FTM_PWM_UPDATE_IN_TICKS, dutyGreen, 0U, true);
		FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 2U, FTM_PWM_UPDATE_IN_TICKS, dutyBlue, 0U, true);

		/* [중요] FreeMASTER 데이터 업데이트: PC와의 통신을 실시간으로 처리합니다 */
		FMSTR_Poll();

		/* 너무 빠른 루프 방지를 위한 짧은 지연 */
		OSIF_TimeDelay(10);

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
