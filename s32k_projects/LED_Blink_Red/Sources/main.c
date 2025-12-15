/* ###################################################################
** Filename    : main.c
** Processor   : S32K1xx
** Abstract    :
** Main module.
** This module contains user's application code.
** ###################################################################*/
/*!
** @file main.c
** @version 01.00
** @brief
** Main module.
** This module contains user's application code.
*/         
/*!
** @addtogroup main_module main module documentation
** @{
*/         
/* MODULE main */

/* Including necessary module. Cpu.h contains other modules needed for compiling.*/
#include "Cpu.h"/* [기존] SDK 헤더 파일 */
#include "clockMan1.h"
#include "pin_mux.h"
#include "device_registers.h"/* [추가됨] FreeMASTER 및 LPUART 헤더 */
#include "freemaster.h"
#include "lpuart1.h"      // lpuart 컴포넌트 추가 후 생성된 헤더
#include "interrupt_manager.h" // 인터럽트 설정을 위해 필요volatile int exit_code = 0;

/* [추가됨] FreeMASTER로 제어할 전역 변수 */
/* volatile 필수: 컴파일러 최적화 방지 */
volatile int g_ledControl = 1;
volatile int exit_code = 0;
/* User includes (#include below this line is not maintained by Processor Expert) */

void disable_wdog(void) {
  WDOG->CNT = 0xD928C520;
  WDOG->TOVAL = 0x0000FFFF;
  WDOG->CS = 0x00002100;
}

/* 딜레이 함수는 이제 거의 안 쓰지만 남겨둡니다 */
void delay_ms(volatile int ms)
{
  volatile int i;
  volatile int j;
  for (i = 0; i < ms; i++) {
      for (j = 0; j < 1000; j++);
  }
}

/*! 
  \brief The main function for the project.
*/
int main(void)
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif/*** End of Processor Expert internal initialization.                    ***/

  /* [중요] 왓치독부터 끕니다 */
  disable_wdog();

  /* 1. 하드웨어 초기화 (SDK) */
  CLOCK_SYS_Init(g_clockManConfigsArr,
                 CLOCK_MANAGER_CONFIG_CNT,
                 g_clockManCallbacksArr,
                 CLOCK_MANAGER_CALLBACK_CNT);
  CLOCK_SYS_UpdateConfiguration(0, CLOCK_MANAGER_POLICY_FORCIBLE);

  PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

  /* [2. FreeMASTER 통신 초기화 시작] */

  /* 2-1. LPUART 드라이버 초기화 */
  /* INST_LPUART1, lpuart1_State 등은 lpuart1.h/c에 정의되어 있음 */
  LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);

  /* 2-2. 인터럽트 연결 (Hooking) */
  /* LPUART1 인터럽트가 발생하면 FreeMASTER 함수(FMSTR_Isr)로 점프하도록 설정 */
  INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, (isr_t*)0);

  /* 2-3. 인터럽트 활성화 */
  INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);

  /* 2-4. FreeMASTER 엔진 시작 */
  FMSTR_Init();


  /* [3. 무한 루프] */
  for(;;) {
      /* [필수] FreeMASTER 폴링: PC에서 온 명령이 있는지 확인 */
      /* 이 함수는 최대한 자주 호출되어야 합니다. */

      /* LED 제어 로직 */
      /* PC에서 g_ledControl 변수를 1로 바꾸면 LED 켜짐 */
      if (g_ledControl == 1) {
          /* LED ON (Active Low: 0일 때 켜짐) */
          PINS_DRV_ClearPins(PTD, 1U << 15);
      }
      else {
          /* LED OFF (Active Low: 1일 때 꺼짐) */
          PINS_DRV_SetPins(PTD, 1U << 15);
      }

      /* [주의] 여기에 delay_ms(500) 같은 긴 딜레이를 넣으면 안 됩니다! */
      /* 통신이 끊깁니다. */
      FMSTR_Poll();
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
