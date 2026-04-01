/*
 * [FILE] main.c
 * [ROLE] System Entry Point and Minimal Scheduler Loop
 * [VERSION] v7 (Layered Architecture)
 * [AUTHOR] sisung
 */

#include "Cpu.h"
#include "wiper_hal.h"
#include "wiper_app.h"

volatile int exit_code = 0;

int main(void)
{
	/*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!!
	 * ***/
#ifdef PEX_RTOS_INIT
	PEX_RTOS_INIT();
#endif
	/*** End of Processor Expert internal initialization. ***/

	/* All hardware init (Clock, Pin, ADC, DMA, FTM, LPIT, CAN, UART, LED) */
	HAL_Wiper_InitAll();

	/* Background loop: Flag-based Scheduling */
	for (;;)
	{
		HAL_Wiper_PollFreemaster();

		/* 10ms periodic control tasks */
		if (timer_10ms_flag)
		{
			timer_10ms_flag = false;

			HAL_Wiper_CheckComm();       /* CAN bus status monitoring */
			Wiper_Diagnostic_Task();     /* 1. Sensor / CAN health check */
			Wiper_Process_Task();        /* 2. ADC filter + mode decision */
			Wiper_Update_Hardware();     /* 3. LED + servo state machine */
		}

		/* Communication task (independent timing) */
		Wiper_Comm_Task();

		if (exit_code != 0)
		{
			break;
		}
	}

	/*** Don't write any code pass this line, or it will be deleted during code
	 * generation. ***/
#ifdef PEX_RTOS_START
	PEX_RTOS_START();
#endif
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
