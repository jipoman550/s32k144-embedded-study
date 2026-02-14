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
/* 1. �ʿ��� ����̹� ��� ���ϵ��� include ���ǿ� �߰��մϴ�. */
#include "Cpu.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "adConv1.h"
#include "flexTimer_pwm1.h"
#include "osif.h"
#include "freemaster.h"
//#include "watchdog1.h"

  volatile int exit_code = 0;

/* --- [Section 1] ���� ���� �� ���� ���� --- */
typedef enum {
  MODE_OFF,
  MODE_INT,
  MODE_LOW,
  MODE_HIGH
} WiperMode_t;

// �������� ���� ���� ����
typedef enum {
    WIPER_IDLE,
    WIPER_MOVING_UP,   // 0 -> 140���� �̵� ��
    WIPER_MOVING_DOWN  // 140 -> 0���� ���� ��
} WiperStep_t;

WiperMode_t currentMode = MODE_OFF;
WiperStep_t currentStep = WIPER_IDLE;
ftm_state_t ftmStateStruct;
uint16_t adcValue = 0;
uint32_t currentTime = 0;
uint32_t lastTime = 0;
volatile uint32_t ms_ticks = 0; // 1ms���� 1�� ������ ��¥ �ð�

/* [�߰�] ����� �� PC ���� ���� */
volatile uint8_t controlSource = 0;    // 0: ��������(ADC), 1: PC(FreeMASTER)
volatile WiperMode_t pcModeRequest = MODE_OFF; // PC���� ���� ��� ����


/* --- [Section 2] ������ --- */
#define POS_0_DEG      800
#define POS_140_DEG    3300
#define INT_WAIT_TIME  3000 // 3�� ���

/* --- [Section 2] LPIT ���ͷ�Ʈ ���� ��ƾ (ISR) --- */
// 1ms���� �ϵ��� �� �Լ��� �ڵ����� ȣ���մϴ�.
void LPIT0_Ch0_IRQHandler(void)
{
    // ���ͷ�Ʈ �÷��׸� ������� ���� ���ͷ�Ʈ�� �߻��մϴ�.
    LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1 << 0));
    ms_ticks++; // �ð� ���� ����
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
    /* �ϵ���� �ʱ�ȭ (���� �ڵ� ����) */
	CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
	CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);

	/* 2. ��ġ�� ���� ��Ȱ��ȭ (Nuclear Option) */
	// ��ġ�� �������Ϳ� ���� �����Ͽ� ����� �����ϰ� EN ��Ʈ�� ���ϴ�.
//	WDOG->CNT = 0xD928C520;              // Unlock Ű 1
//	WDOG->CNT = 0xD928C520;              // Unlock Ű 2
//	while((WDOG->CS & (1U << 11)) == 0); // ��� ������ ������ ���
//	WDOG->CS &= ~(1U << 7);              // EN(Enable) ��Ʈ ���� (������ ����)

	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
	ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);
	FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &ftmStateStruct);
	FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);

	// LPIT Ÿ�̸� �ʱ�ȭ �� ����
	LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
	LPIT_DRV_InitChannel(INST_LPIT1, 0, &lpit1_ChnConfig0);
	LPIT_DRV_StartTimerChannels(INST_LPIT1, (1 << 0));

	// 1. LPUART1 ���� ���� �ʱ�ȭ
	LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);

	// 2. LPUART ���ͷ�Ʈ�� FreeMASTER ���� ��ƾ ����
	// UART�� �����Ͱ� ������ CPU�� FMSTR_Isr �Լ��� �ٷ� �����ϰ� ����ϴ�.
	INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, NULL);

	// 3. �ϵ���� �������� LPUART ���ͷ�Ʈ ��� ����
	INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);

	// 4. FreeMASTER ����̹� ���� ���� �ʱ�ȭ
	FMSTR_Init();

	// 5. �ý��� ��ü ���ͷ�Ʈ Ȱ��ȭ (LPIT�� LPUART ��θ� ���� �ʼ�)
	INT_SYS_EnableIRQGlobal();

    // ������ ������ ���� �ð� ���� ����
    uint32_t moveDuration = 500; // �⺻ �̵� �ð�

	/* [Section 3] ����Ʈ ������ ���� ���� */
	for(;;) {
		// FreeMASTER ��� ó�� (���� ���� Ȥ�� ���� ���߿� ��ġ)
		FMSTR_Poll();

		currentTime = ms_ticks;

		/* [STEP 1] ADC �б� (�������� �� ȹ��) */
		ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);
		while(ADC_DRV_GetConvCompleteFlag(INST_ADCONV1, 0U) == false);
		ADC_DRV_GetChanResult(INST_ADCONV1, 0U, &adcValue);

		/* [STEP 2] ADC ���� ���� ���� ���� (�����缭 �ؼ�) */
		if (controlSource == 0) {
			/* [���� ����] ��������(ADC) ���� ��� */
			if (adcValue < 500)       currentMode = MODE_OFF;
			else if (adcValue < 2000) currentMode = MODE_INT;
			else if (adcValue < 3500) currentMode = MODE_LOW;
			else                      currentMode = MODE_HIGH;
		}
		else {
			/* [���ο� ����] PC(FreeMASTER) ���� ��� */
			// PC���� pcModeRequest ���� �ٲٸ� ��� ������ ��尡 �ٲ�ϴ�.
			currentMode = pcModeRequest;
		}

		/* [STEP 3] ������ ���º� ���� ���� */
		switch(currentMode) {
			case MODE_OFF:
				FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
				currentStep = WIPER_IDLE;
				break;

			case MODE_INT:    moveDuration = 500; break;
			case MODE_LOW:    moveDuration = 800; break;
			case MODE_HIGH:   moveDuration = 300; break;
		}

		// ������ �պ� ���� (MODE_OFF�� �ƴ� ���� �۵�)
		if (currentMode != MODE_OFF) {
			// 1. ��� ���� -> ���� �̵� ����
			if (currentStep == WIPER_IDLE) {
				currentStep = WIPER_MOVING_UP;
				lastTime = currentTime;
				FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_140_DEG, 0U, true);
			}
			// 2. ���� �̵� �Ϸ� üũ -> �Ʒ��� �̵� ����
			else if (currentStep == WIPER_MOVING_UP) {
				if (currentTime - lastTime >= moveDuration) {
					currentStep = WIPER_MOVING_DOWN;
					lastTime = currentTime;
					FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
				}
			}
			// 3. �Ʒ��� �̵� �Ϸ� üũ -> IDLE�� ���� (��� �ð� ����)
			else if (currentStep == WIPER_MOVING_DOWN) {
				uint32_t waitTarget = (currentMode == MODE_INT) ? (moveDuration + INT_WAIT_TIME) : moveDuration;

				if (currentTime - lastTime >= waitTarget) {
					currentStep = WIPER_IDLE; // �ٽ� ó������ ������ �غ� �Ϸ�
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
