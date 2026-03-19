/*
 * [FILE] main.c
 * [ROLE] 스마트 와이퍼 시스템의 메인 제어 모듈
 * [VERSION] v5 (Refactored & DSP Base)
 * [AUTHOR] S32K144 Embedded Mentor
 */

/* ---------------------------------------------------------------- 프로젝트 헤더 포함 */
#include "Cpu.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "adConv1.h"
#include "flexTimer_pwm1.h"
#include "osif.h"
#include "freemaster.h"
#include "dmaController1.h"
#include "lpit1.h"

volatile int exit_code = 0;

/* ---------------------------------------------------------------- 전역 상수 및 정의 */
#define POS_0_DEG      (800U)   /* 서보모터 0도 위치 (PWM Duty) */
#define POS_140_DEG    (3300U)  /* 서보모터 140도 위치 (PWM Duty) */
#define INT_WAIT_TIME  (3000U)  /* 간헐적 모드(INT) 대기 시간 (3000ms) */

#define FILTER_SIZE 8U  /* 8개의 샘플을 평균냄 (반응성과 부드러움의 타협점) */

#define ADC_MIN_SAFE 50U    /* 50 미만은 단선 혹은 지엔디 쇼트로 간주 */
#define ADC_MAX_SAFE 4040U  /* 4040 초과는 전원 쇼트로 간주 */

#define MAX_DELTA 1500U  /* 10ms 동안 허용 가능한 최대 변화량 : 이거는 어떻게 값을 정하는거지? 사람이 휙휙돌리는거랑 점퍼선 불량이랑 차이를 구분해야할 것 같은데 */

/* ---------------------------------------------------------------- 하드웨어 초기화 함수 원형 */
void System_Init(void);     /* SCG(클럭), PORT(핀) 초기화 */
void Sensor_Init(void);     /* ADC0, eDMA 초기화 */
void Motor_Init(void);      /* FTM1(PWM) 초기화 */
void Timer_Init(void);      /* LPIT0(10ms 타이머) 초기화 */
void Comm_Init(void);       /* LPUART1, FreeMASTER 초기화 */
void LED_Init(void);        /* RGB LED GPIO 초기화 */

/* ---------------------------------------------------- 진단 및 로직 함수 원형 */
bool Is_Sensor_Healthy(uint16_t value);
bool Is_Signal_Stable(uint16_t current);

/* ---------------------------------------------------------------- 사용자 정의 자료형 */
typedef enum
{
    MODE_OFF  = 0U,
    MODE_INT  = 1U,
    MODE_LOW  = 2U,
    MODE_HIGH = 3U
} WiperMode_t;

typedef enum
{
    WIPER_IDLE         = 0U,
    WIPER_MOVING_UP    = 1U,
    WIPER_MOVING_DOWN  = 2U
} WiperStep_t;

/* ---------------------------------------------------------------- 전역 변수 설정 */
static volatile WiperMode_t currentMode = MODE_OFF;
static volatile WiperStep_t currentStep = WIPER_IDLE;
static ftm_state_t ftmStateStruct;

/* DMA(배달부)가 ADC0 결과를 실시간으로 복사해올 목적지 주소 */
volatile uint16_t adcValue = 0U;

static uint16_t adcBuffer[FILTER_SIZE] = {0U, }; /* 샘플 저장소 */
static uint32_t adcSum = 0U;                     /* 샘플들의 합계 */
static uint8_t  bufferIdx = 0U;                  /* 현재 저장 위치 */

volatile uint16_t filteredAdc = 0U;              /* 최종 필터링된 값 */

/* 시스템 모니터링용 변수 */
volatile uint32_t ms_ticks = 0U;
volatile uint32_t loop_cnt = 0U;

/* 통신(FreeMASTER) 제어용 변수 */
volatile uint8_t controlSource = 0U;     /* 0: 가변저항, 1: PC 제어 */
volatile WiperMode_t pcModeRequest = MODE_OFF;

/* Delta Check 변수 */
static uint16_t lastValue = 0U;

/* ---------------------------------------------------------------- 인터럽트 서비스 루틴 */
void LPIT0_Ch0_IRQHandler(void)
{
    /* [ROLE 1] 인터럽트 플래그 클리어 */
    LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1U << 0U));

    ms_ticks += 10U;

    /* 하드웨어 상태 체크 (Failsafe) */
    bool healthy = Is_Sensor_Healthy(adcValue);
    bool stable = Is_Signal_Stable(adcValue);

    bool is_fault = (!healthy || !stable);

    if (is_fault)
	{
		/* [SAFE-STATE 전략]
		 * 센서가 정상이 아닐 때는 모드만 OFF로 바꾸고 '아래 로직을 계속 태워야' 함.
		 * 그래야 [ROLE 3]에서 와이퍼를 0도로 복귀시키는 명령이 실제로 하드웨어에 전달됨. */
		currentMode = MODE_OFF;
	}
    else
    {
    	/* --- [이동 평균 필터 로직] : 정상일 때만 버퍼 업데이트 --- */
		/* 1. 합계에서 가장 오래된 데이터 제거 */
		adcSum -= adcBuffer[bufferIdx];

		/* 2. DMA가 가져온 최신 ADC 값을 버퍼에 저장 */
		adcBuffer[bufferIdx] = adcValue;

		/* 3. 합계에 최신 데이터 추가 */
		adcSum += adcBuffer[bufferIdx];

		/* 4. 버퍼 인덱스 이동 (순환 구조) */
		bufferIdx = (uint8_t)((bufferIdx + 1U) % FILTER_SIZE);

		/* 5. 평균 계산 (나누기 8 대신 >> 3 사용) */
		filteredAdc = (uint16_t)(adcSum >> 3U);

		/* [ROLE 2] 입력 데이터 기반 모드 결정 (Input Logic) */
		if (controlSource == 0U)
		{
			if (filteredAdc < 500U)            currentMode = MODE_OFF;
			else if (filteredAdc < 2000U)      currentMode = MODE_INT;
			else if (filteredAdc < 3500U)      currentMode = MODE_LOW;
			else                               currentMode = MODE_HIGH;
		}
		else
		{
			currentMode = pcModeRequest;
		}
    }

    /* ROLE 2.5: LED 시각적 피드백 (Visual Feedback) (항상 실행) */
    /* 1. 먼저 모든 LED를 꺼서 초기화 (Active Low이므로 PSOR로 1 출력) */
    PTD->PSOR |= (1 << 0) | (1 << 15) | (1 << 16);

    /* 2. 현재 모드에 따라 특정 LED만 켜기
	 * PCOR(Port Clear Output Register)는 1을 적어 넣은 비트만 0(Low)으로 떨어뜨립니다! */
	if (currentMode == MODE_OFF)
	{
		/* 아무것도 안 함 (이미 위에서 다 껐음) */
	}
	else if (currentMode == MODE_INT)
	{
		PTD->PCOR |= (1 << 0);  /* Blue ON */
	}
	else if (currentMode == MODE_LOW)
	{
		PTD->PCOR |= (1 << 16); /* Green ON */
	}
	else if (currentMode == MODE_HIGH)
	{
		PTD->PCOR |= (1 << 15); /* Red ON */
	}

    /* [ROLE 3] 와이퍼 구동 상태 머신 (Control Logic)
     * 여기서 currentMode가 MODE_OFF이면 POS_0_DEG로 이동함.
     * 고장 시에도 이 로직이 돌아가야 와이퍼가 원위치로 복귀함. */
    static uint32_t stepCounter = 0U;
    uint32_t moveDuration = 0U;

    if (currentMode == MODE_OFF)
    {
        (void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
        currentStep = WIPER_IDLE;
        stepCounter = 0U;
    }
    else
    {
        if (currentMode == MODE_INT)           moveDuration = 500U;
        else if (currentMode == MODE_LOW)      moveDuration = 800U;
        else                                   moveDuration = 300U;

        if (currentStep == WIPER_IDLE)
        {
            currentStep = WIPER_MOVING_UP;
            stepCounter = 0U;
            (void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_140_DEG, 0U, true);
        }

        /* 10ms마다 상태 머신 체류 시간 누적 */
        stepCounter += 10U;

        if (currentStep == WIPER_MOVING_UP)
        {
            if (stepCounter >= moveDuration)
            {
                currentStep = WIPER_MOVING_DOWN;
                stepCounter = 0U;
                (void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U, FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG, 0U, true);
            }
        }
        else if (currentStep == WIPER_MOVING_DOWN)
        {
            uint32_t waitTarget = (currentMode == MODE_INT) ? (moveDuration + INT_WAIT_TIME) : moveDuration;

            if (stepCounter >= waitTarget)
            {
                currentStep = WIPER_IDLE;
                stepCounter = 0U;
            }
        }
        else
        {
            currentStep = WIPER_IDLE;
        }
    }
}

/* ---------------------------------------------------------------- 메인 함수 */
/*!
  \brief The main function for the project.
*/
int main(void)
{
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

    /* [Refactoring] 하드웨어 모듈별 초기화 (OS 부팅 시퀀스화) */
    System_Init();
    Sensor_Init();
    Motor_Init();
    Timer_Init();
    Comm_Init();
    LED_Init();

    /* 전역 인터럽트 허용: 모든 하드웨어 셋팅이 끝난 후 심장 박동 시작 */
    INT_SYS_EnableIRQGlobal();

    /* 백그라운드 무한 루프 */
    for(;;)
    {
        FMSTR_Poll();
        loop_cnt++;

        if(exit_code != 0)
        {
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

/* ---------------------------------------------------------------- 하드웨어 초기화 함수 구현 */

void System_Init(void)
{
    /* SCG (System Clock Generator) & PCC (Peripheral Clock Controller)
     * MCU의 메인 클럭 분배 및 핀(PORT) MUX 설정 */
    (void)CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
    (void)CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
    (void)PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
}

void Sensor_Init(void)
{
    /* ADC (Analog-to-Digital Converter) & eDMA (Direct Memory Access)
     * 가변저항의 아날로그 값을 디지털로 변환하고, CPU 개입 없이 DMA가 메모리로 배달 */
    (void)EDMA_DRV_Init(&dmaController1_State, &dmaController1_InitConfig0, edmaChnStateArray, edmaChnConfigArray, 1U);
    ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);
    (void)EDMA_DRV_ConfigSingleBlockTransfer(0U,
                                            EDMA_TRANSFER_PERIPH2PERIPH,
                                            (uint32_t)&(ADC0->R[0]),
                                            (uint32_t)&adcValue,
                                            EDMA_TRANSFER_SIZE_2B,
                                            2U);
    (void)EDMA_DRV_StartChannel(0U);
    ADC_DRV_ConfigChan(INST_ADCONV1, 0U, &adConv1_ChnConfig0);
}

void Motor_Init(void)
{
	/* ------------------------------------------------------------------------ */
	/* [디버깅용 강제 코드] S32DS GUI가 말을 안 들을 때를 대비한 하드웨어 직접 제어 */

	/* 1. PORTC 클럭 켜기 (건물 C동에 전기 공급) */
	// PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;


    /* FTM (FlexTimer Module)
     * 서보모터를 움직이기 위한 50Hz PWM 신호 생성 */
    (void)FTM_DRV_Init(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_InitConfig, &ftmStateStruct);
    (void)FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);


	/* 2. PTC0(D9) 핀을 ALT2(FTM0_CH0) 모드로 강제 설정
	 * [RM 참조] PTC0 핀의 MUX 값을 2로 설정하면 모터 전용 핀으로 변신합니다! */
	// PORTC->PCR[0] = PORT_PCR_MUX(2);
	/* ------------------------------------------------------------------------ */
}

void Timer_Init(void)
{
    /* LPIT (Low Power Interrupt Timer)
     * 10ms마다 인터럽트를 발생시켜 시스템의 State Machine을 굴림 */
    (void)LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
    (void)LPIT_DRV_InitChannel(INST_LPIT1, 0, &lpit1_ChnConfig0);
    LPIT_DRV_StartTimerChannels(INST_LPIT1, (1U << 0U));
}

void Comm_Init(void)
{
    /* LPUART & FreeMASTER
     * PC와 통신하여 변수들을 실시간으로 모니터링하고 제어 */
    (void)LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);
    (void)INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, (isr_t*)0);
    INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);
    FMSTR_Init();
}

void LED_Init(void)
{
    /* 1. PORTD 클럭 활성화 (PCC 레지스터)
     * MCU는 전력 소모를 막기 위해 기본적으로 모든 포트의 전원을 꺼둡니다.
     * PORTD의 Clock Gate Control(CGC) 비트를 1로 만들어 심장을 뛰게 합니다. */
    PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;

    /* 2. 핀 기능(Pin Muxing)을 GPIO로 설정 (PORT 레지스터)
     * 이 핀을 다른 특수 기능이 아닌 일반 입출력(GPIO, MUX=1)으로 쓰겠다고 선언합니다. */
    PORTD->PCR[0]  = PORT_PCR_MUX(1); /* Blue */
    PORTD->PCR[15] = PORT_PCR_MUX(1); /* Red */
    PORTD->PCR[16] = PORT_PCR_MUX(1); /* Green */

    /* 3. 데이터 방향(Direction)을 출력(Output)으로 설정 (GPIO 레지스터)
     * PDDR(Port Data Direction Register)의 해당 비트를 1로 만들면 출력(Output) 모드가 됩니다. */
    PTD->PDDR |= (1 << 0) | (1 << 15) | (1 << 16);

    /* 4. 초기 상태: 모든 LED 끄기
     * Active Low이므로, 핀으로 1(High, 5V)을 출력해야 전위차가 없어져서 불이 꺼집니다.
     * PSOR(Port Set Output Register)는 1을 적어 넣은 비트를 High 상태로 만듭니다. */
    PTD->PSOR |= (1 << 0) | (1 << 15) | (1 << 16);
}

/* ---------------------------------------------------- 진단 함수 구현부 */
/**
 * @brief 센서의 물리적 연결 상태를 체크합니다.
 * @param value 현재 ADC 생데이터 (Raw Data)
 * @return true: 정상, false: 단선 혹은 쇼트 의심
 */
bool Is_Sensor_Healthy(uint16_t value)
{
    if (value < ADC_MIN_SAFE || value > ADC_MAX_SAFE)
    {
    	return false;
    }
    return true;
}

bool Is_Signal_Stable(uint16_t current)
{
	/* Delta Check: 이전 값과의 차이를 계산 */
    uint16_t delta = (current > lastValue) ? (current - lastValue) : (lastValue - current);

    /* [SOPHISTICATED TIP]
	 * 결함 상태에서도 lastValue는 계속 업데이트해줘야 단선 복구 시 오진단을 막음 */
	lastValue = current;

	return (delta <= MAX_DELTA);
}

/* END main */
