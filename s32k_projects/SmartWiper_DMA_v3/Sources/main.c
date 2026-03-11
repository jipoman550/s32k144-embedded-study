/* ###################################################################
** Filename    : main.c
** Processor   : S32K1xx
** Abstract    :
** Main module.
** This module contains user's application code.
** Settings    :
** Contents    :
** No public methods
**
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
#include "Cpu.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "adConv1.h"
#include "flexTimer_pwm1.h"
#include "osif.h"
#include "freemaster.h"
#include "dmaController1.h"         /* DMA 드라이버 추가 */

volatile int exit_code = 0;

/* --- [User Section 1] 상태 정의 및 전역 변수 --- */
typedef enum {
    MODE_OFF,
    MODE_INT,
    MODE_LOW,
    MODE_HIGH
} WiperMode_t;

typedef enum {
    WIPER_IDLE,
    WIPER_MOVING_UP,
    WIPER_MOVING_DOWN
} WiperStep_t;

WiperMode_t currentMode = MODE_OFF;
WiperStep_t currentStep = WIPER_IDLE;
ftm_state_t ftmStateStruct;

/* [DMA의 보물창고] 이 변수는 이제 CPU가 아닌 DMA 배달부가 실시간으로 업데이트합니다. */
volatile uint16_t adcValue = 0;

uint32_t currentTime = 0;
uint32_t lastTime = 0;
volatile uint32_t ms_ticks = 0;

volatile uint8_t controlSource = 0;    /* 0: 가변저항, 1: PC(FreeMASTER) */
volatile WiperMode_t pcModeRequest = MODE_OFF;

#define POS_0_DEG      800
#define POS_140_DEG    3300
#define INT_WAIT_TIME  3000 // 3초 대기

/* --- [User Section 2] 인터럽트 서비스 루틴 (ISR) --- */
void LPIT0_Ch0_IRQHandler(void)
{
    LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1 << 0));
    ms_ticks++;
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
  uint32_t moveDuration = 500;

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                 /* Initialization of the selected RTOS. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* MPU(메모리 보호 유닛)를 비활성화하여 DMA의 RAM 접근권을 허용합니다. */
  MPU->CESR &= ~MPU_CESR_VLD_MASK;

  /* --- [1. 하드웨어 기본 초기화] --- */
  CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
  CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
  PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

  /* --- [2. eDMA 엔진 가동] --- */
  // 배달부(DMA)를 대기시키고 채널 상태를 초기화합니다.
  /* dmaController1이라는 이름에 맞춰서 인자들을 수정합니다. */
  EDMA_DRV_Init(&dmaController1_State,
                &dmaController1_InitConfig0,
				edmaChnStateArray,
				edmaChnConfigArray,
                1); // 초기화할 채널 수 (보통 1 또는 설정된 채널 수)

  /* --- [3. ADC 초기화] --- */
  ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);

  /* --- [4. DMA 배달 경로(TCD) 설정] --- */
  // ADC0 공장에서 물건(RA 레지스터)이 나오면 -> 내 변수(adcValue)로 2바이트를 배달하라!
  // ※ 주의: PE에서 설정한 ADC 전용 채널 번호(예: 0U)를 사용하세요.

  // 기존의 복잡한 구조체 채우기 및 PushConfigToReg를 이 한 줄로 대체합니다.
  // 0U: 가상 채널 번호
  // EDMA_TRANSFER_PERIPH2MEM: ADC(주변장치)에서 메모리로 배달
  // (uint32_t)&(ADC0->R[0]): 출발지
  // (uint32_t)&adcValue: 목적지
  // EDMA_TRANSFER_SIZE_2B: 2바이트씩 배달
  // 2U: 한 번의 요청에 2바이트를 옮김 (Minor Loop)

  /* EDMA_TRANSFER_PERIPH2MEM 대신 EDMA_TRANSFER_PERIPH2PERIPH를 사용합니다. */
  /* 이 타입은 원본(ADC)과 목적지(adcValue) 주소 둘 다 고정(Offset = 0)시킵니다. */

  EDMA_DRV_ConfigSingleBlockTransfer(0U,
                                     EDMA_TRANSFER_PERIPH2PERIPH, // 목적지 주소 고정!
                                     (uint32_t)&(ADC0->R[0]),
                                     (uint32_t)&adcValue,
                                     EDMA_TRANSFER_SIZE_2B,
                                     2U);

  /* --- [5. 시스템 가동 시작] --- */
  EDMA_DRV_StartChannel(0U);                     			 /* DMA 배달 시작 */
  //DMA0->ERQ |= (1U << 0U); // 0번 채널의 하드웨어 요청 수신을 강제로 허용 (ERQ0 = 1)
  ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0); /* 첫 번째 ADC 트리거 시작 */

  /* 나머지 주변장치 초기화 */
  FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &ftmStateStruct);
  FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);
  LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
  LPIT_DRV_InitChannel(INST_LPIT1, 0, &lpit1_ChnConfig0);
  LPIT_DRV_StartTimerChannels(INST_LPIT1, (1 << 0));
  LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);
  INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, NULL);
  INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);
  FMSTR_Init();
  INT_SYS_EnableIRQGlobal();

  /* --- [스마트 와이퍼 실행 루프] --- */
  for(;;) {
    FMSTR_Poll();
    currentTime = ms_ticks;

    /* * [DMA의 이점]
     * 이전 코드에 있던 'while(ADC 완료 대기)' 줄이 완전히 삭제되었습니다.
     * CPU는 기다리지 않고 바로 아래 로직을 수행하며, adcValue는 하드웨어가 알아서 채워줍니다.
     */

    /* [STEP 1] 모드 결정 로직 */
    if (controlSource == 0) {
        if (adcValue < 500)       currentMode = MODE_OFF;
        else if (adcValue < 2000)  currentMode = MODE_INT;
        else if (adcValue < 3500)  currentMode = MODE_LOW;
        else                       currentMode = MODE_HIGH;
    } else {
        currentMode = pcModeRequest;
    }

    /* [STEP 2] 상태별 동작 수행 (Non-blocking State Machine) */
    switch(currentMode) {
        case MODE_OFF:
            FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
            currentStep = WIPER_IDLE;
            break;
        case MODE_INT:  moveDuration = 500; break;
        case MODE_LOW:  moveDuration = 800; break;
        case MODE_HIGH: moveDuration = 300; break;
    }

    /* [STEP 3] 와이퍼 왕복 제어 */
    if (currentMode != MODE_OFF) {
        if (currentStep == WIPER_IDLE) {
            currentStep = WIPER_MOVING_UP;
            lastTime = currentTime;
            FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_140_DEG, 0U, true);
        }
        else if (currentStep == WIPER_MOVING_UP) {
            if (currentTime - lastTime >= moveDuration) {
                currentStep = WIPER_MOVING_DOWN;
                lastTime = currentTime;
                FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
            }
        }
        else if (currentStep == WIPER_MOVING_DOWN) {
            uint32_t waitTarget = (currentMode == MODE_INT) ? (moveDuration + INT_WAIT_TIME) : moveDuration;
            if (currentTime - lastTime >= waitTarget) {
                currentStep = WIPER_IDLE;
            }
        }
    }

    if(exit_code != 0) break;
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
