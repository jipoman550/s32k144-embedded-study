/*
 * [FILE] main.c
 * [ROLE] 스마트 와이퍼 시스템의 메인 제어 모듈
 * [VERSION] v6 (Refactored & DSP Base + ISR Diet + Function Wrapping)
 * [AUTHOR] sisung
 */

/* ---------------------------------------------------------------- 프로젝트
 * 헤더 포함 */
#include "Cpu.h"
#include "adConv1.h"
#include "clockMan1.h"
#include "dmaController1.h"
#include "flexTimer_pwm1.h"
#include "flexcan_hw.h"
#include "freemaster.h"
#include "lpit1.h"
#include "osif.h"
#include "pin_mux.h"

volatile int exit_code = 0;
volatile uint8_t errorResetRequest = 0U; /* FreeMASTER 수동 복구 요청용 */

/* ---------------------------------------------------------------- 전역 상수 및
 * 정의 */
#define POS_0_DEG (800U)	  /* 서보모터 0도 위치 (PWM Duty) */
#define POS_140_DEG (3300U)	  /* 서보모터 140도 위치 (PWM Duty) */
#define INT_WAIT_TIME (3000U) /* 간헐적 모드(INT) 대기 시간 (3000ms) */

#define FILTER_SIZE 8U /* 8개의 샘플을 평균냄 (반응성과 부드러움의 타협점) */

#define ADC_MIN_SAFE 50U   /* 50 미만은 단선 혹은 지엔디 쇼트로 간주 */
#define ADC_MAX_SAFE 4040U /* 4040 초과는 전원 쇼트로 간주 */

#define MAX_DELTA 1500U /* 10ms 동안 허용 가능한 최대 변화량 */

/* [MISRA-C] 임계값 및 핀 번호 상수화 (매직 넘버 제거) */
#define ADC_THRES_OFF_INT  500U
#define ADC_THRES_INT_LOW  2000U
#define ADC_THRES_LOW_HIGH 3500U

#define LED_PIN_BLUE  0U
#define LED_PIN_RED   15U
#define LED_PIN_GREEN 16U

/* ---------------------------------------------------------------- 하드웨어
 * 초기화 함수 원형 */
void System_Init(void); /* SCG(클럭), PORT(핀) 초기화 */
void Sensor_Init(void); /* ADC0, eDMA 초기화 */
void Motor_Init(void);	/* FTM1(PWM) 초기화 */
void Timer_Init(void);	/* LPIT0(10ms 타이머) 초기화 */
void Comm_Init(void);	/* LPUART1, FreeMASTER 초기화 */
void LED_Init(void);	/* RGB LED GPIO 초기화 */

/* ---------------------------------------------------- 진단 및 저수준 함수 원형
 */
bool Is_Sensor_Healthy(uint16_t value);
bool Is_Signal_Stable(uint16_t current);

/* ---------------------------------------------------- 래핑 태스크 함수 원형
 * (Input → Process → Output 구조) */
void Wiper_Diagnostic_Task(void);  /* 센서/CAN 건강 체크 → is_fault 판정 */
void Wiper_Process_Task(void);	   /* ADC 필터링 + 모드 결정 */
void Wiper_Update_Hardware(void);  /* LED 제어 + 서보 상태 머신 */
void Wiper_Comm_Task(void);	   /* CAN 송수신 + Recovery */

/* ---------------------------------------------------------------- 사용자 정의
 * 자료형 */
typedef enum
{
	MODE_OFF = 0U,
	MODE_INT = 1U,
	MODE_LOW = 2U,
	MODE_HIGH = 3U
} WiperMode_t;

typedef enum
{
	WIPER_IDLE = 0U,
	WIPER_MOVING_UP = 1U,
	WIPER_MOVING_DOWN = 2U
} WiperStep_t;

/* ---------------------------------------------------------------- 전역 변수
 * 설정 */
static WiperMode_t currentMode = MODE_OFF;
static WiperStep_t currentStep = WIPER_IDLE;
static ftm_state_t ftmStateStruct;

/* CAN 전송 주기 관리용 변수 */
static uint32_t lastCanTxTime = 0U;

/* DMA(배달부)가 ADC0 결과를 실시간으로 복사해올 목적지 주소 */
volatile uint16_t adcValue = 0U;

static uint16_t adcBuffer[FILTER_SIZE] = {
	0U,
};							   /* 샘플 저장소 */
static uint32_t adcSum = 0U;   /* 샘플들의 합계 */
static uint8_t bufferIdx = 0U; /* 현재 저장 위치 */

volatile uint16_t filteredAdc = 0U; /* 최종 필터링된 값 */

/* 시스템 모니터링용 변수 */
volatile uint32_t ms_ticks = 0U; // 오버프로우 이슈
volatile uint32_t loop_cnt = 0U;

/* 통신(FreeMASTER) 제어용 변수 */
volatile uint8_t controlSource = 0U; /* 0: 가변저항, 1: PC 제어 */
volatile WiperMode_t pcModeRequest = MODE_OFF;

/* Delta Check 변수 */
static uint16_t lastValue = 0U;

/* [ISR Diet] ISR→Main Loop 신호용 플래그 */
volatile bool timer_10ms_flag = false;

/* 와이퍼 상태 머신용 카운터 */
static uint32_t stepCounter = 0U;

/* [Diagnostic] 고장 판정 플래그 (함수 간 공유) */
static bool is_fault = false;

/* ---------------------------------------------------------------- 인터럽트
 * 서비스 루틴 (ISR Diet: "벨"만 울리고 즉시 종료) */
void LPIT0_Ch0_IRQHandler(void)
{
	LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1U << 0U));
	ms_ticks += 10U;
	timer_10ms_flag = true;
}

/* ================================================================
 * 래핑 태스크 함수 구현부 (Input → Process → Output)
 * ================================================================ */

/* ----------------------------------------------------------------
 * [1단계] Wiper_Diagnostic_Task: 센서/CAN 건강 체크
 * - 센서 단선/쇼트, 신호 안정성, CAN 통신 에러를 통합 진단
 * - 결과: is_fault 플래그 갱신 → 고장 시 MODE_OFF 강제 적용
 * ---------------------------------------------------------------- */
void Wiper_Diagnostic_Task(void)
{
	bool healthy = Is_Sensor_Healthy(adcValue);
	bool stable = Is_Signal_Stable(adcValue);

	/* 센서 이상 + CAN 통신 에러 = 통합 고장 판정 */
	is_fault = (!healthy || !stable || is_can_failsafe);

	if (is_fault)
	{
		/* [SAFE-STATE] 고장 시 모드만 OFF로 전환
		 * 아래 Update_Hardware()에서 와이퍼를 0도로 복귀시킴 */
		currentMode = MODE_OFF;
	}
}

/* ----------------------------------------------------------------
 * [2단계] Wiper_Process_Task: ADC 필터링 + 모드 결정
 * - 정상 상태일 때만 이동 평균 필터 갱신 및 모드 결정
 * - 고장 시에는 Diagnostic에서 이미 MODE_OFF로 설정했으므로 스킵
 * ---------------------------------------------------------------- */
void Wiper_Process_Task(void)
{
	if (!is_fault)
	{
		/* --- [이동 평균 필터 로직] --- */
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

		/* --- [모드 결정 로직] --- */
		if (controlSource == 0U)
		{
			if (filteredAdc < ADC_THRES_OFF_INT)
			{
				currentMode = MODE_OFF;
			}
			else if (filteredAdc < ADC_THRES_INT_LOW)
			{
				currentMode = MODE_INT;
			}
			else if (filteredAdc < ADC_THRES_LOW_HIGH)
			{
				currentMode = MODE_LOW;
			}
			else
			{
				currentMode = MODE_HIGH;
			}
		}
		else
		{
			currentMode = pcModeRequest;
		}
	}
}

/* ----------------------------------------------------------------
 * [3단계] Wiper_Update_Hardware: LED 제어 + 서보 상태 머신
 * - LED: CAN 에러 시 빨간불 깜빡임, 정상 시 모드별 색상
 * - 서보: MODE별 PWM 듀티 업데이트 및 UP/DOWN 상태 전이
 * ---------------------------------------------------------------- */
void Wiper_Update_Hardware(void)
{
	/* === LED 시각적 피드백 === */
	/* 1. 모든 LED 끄기 (Active Low이므로 PSOR로 High 출력) */
	PTD->PSOR |= (1U << LED_PIN_BLUE) | (1U << LED_PIN_RED) | (1U << LED_PIN_GREEN);

	/* 2. 에러 상태 혹은 현재 모드에 따라 LED 제어 */
	if (is_can_failsafe)
	{
		/* [Failsafe] CAN 에러 시 빨간 LED 깜빡임 (500ms 주기) */
		if ((ms_ticks % 1000U) < 500U)
		{
			PTD->PCOR |= (1U << LED_PIN_RED); /* Red ON */
		}
	}
	else
	{
		if (currentMode == MODE_OFF)
		{
			/* 아무것도 안 함 (이미 위에서 다 껐음) */
		}
		else if (currentMode == MODE_INT)
		{
			PTD->PCOR |= (1U << LED_PIN_BLUE); /* Blue ON */
		}
		else if (currentMode == MODE_LOW)
		{
			PTD->PCOR |= (1U << LED_PIN_GREEN); /* Green ON */
		}
		else if (currentMode == MODE_HIGH)
		{
			PTD->PCOR |= (1U << LED_PIN_RED); /* Red ON */
		}
	}

	/* === 와이퍼 구동 상태 머신 === */
	uint32_t moveDuration = 0U;

	if (currentMode == MODE_OFF)
	{
		(void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
									   FTM_PWM_UPDATE_IN_DUTY_CYCLE, POS_0_DEG,
									   0U, true);
		currentStep = WIPER_IDLE;
		stepCounter = 0U;
	}
	else
	{
		if (currentMode == MODE_INT)
		{
			moveDuration = 500U;
		}
		else if (currentMode == MODE_LOW)
		{
			moveDuration = 800U;
		}
		else
		{
			moveDuration = 300U;
		}

		if (currentStep == WIPER_IDLE)
		{
			currentStep = WIPER_MOVING_UP;
			stepCounter = 0U;
			(void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
										   FTM_PWM_UPDATE_IN_DUTY_CYCLE,
										   POS_140_DEG, 0U, true);
		}

		/* 10ms마다 상태 머신 체류 시간 누적 */
		stepCounter += 10U;

		if (currentStep == WIPER_MOVING_UP)
		{
			if (stepCounter >= moveDuration)
			{
				currentStep = WIPER_MOVING_DOWN;
				stepCounter = 0U;
				(void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
											   FTM_PWM_UPDATE_IN_DUTY_CYCLE,
											   POS_0_DEG, 0U, true);
			}
		}
		else if (currentStep == WIPER_MOVING_DOWN)
		{
			uint32_t waitTarget = (currentMode == MODE_INT)
									  ? (moveDuration + INT_WAIT_TIME)
									  : moveDuration;

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

/* ----------------------------------------------------------------
 * [통신] Wiper_Comm_Task: CAN 송수신 + Recovery
 * - 100ms 주기 CAN 데이터 송신
 * - 수동 리셋(Latch) 복구 처리
 * - 수신 폴링 (매 루프 실행)
 * ---------------------------------------------------------------- */
void Wiper_Comm_Task(void)
{
	/* [ROLE A] 100ms마다 CAN 데이터 송신 */
	if ((ms_ticks - lastCanTxTime) >= 100U)
	{
		/* [Recovery] 수동 리셋(Latch) 로직 */
		if (is_can_failsafe)
		{
			if (errorResetRequest == 1U)
			{
				is_can_failsafe = false;
				can_ack_err_cnt = 0;
				errorResetRequest = 0U;
			}
		}

		FlexCAN0_Send_Wiper_Data(filteredAdc,
								(uint8_t)currentMode,
								(uint8_t)currentStep);
		lastCanTxTime = ms_ticks;
	}

	/* [ROLE B] 수신 폴링 (매 루프마다 실행) */
	if (FLEXCAN_DRV_GetTransferStatus(INST_CANCOM1, 1U) == STATUS_SUCCESS)
	{
		can_rx_count++;

		last_rx_data[0] = rx_msg.data[0];
		last_rx_data[1] = rx_msg.data[1];
		last_rx_data[2] = rx_msg.data[2];
		last_rx_data[3] = rx_msg.data[3];

		/* 다시 수신 대기 상태로 만들어 놓음 */
		FLEXCAN_DRV_Receive(INST_CANCOM1, 1U, &rx_msg);
	}
}

/* ---------------------------------------------------------------- 메인 함수 */
/*!
 \brief The main function for the project.
 */
int main(void)
{
	/*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!!
	 * ***/
#ifdef PEX_RTOS_INIT
	PEX_RTOS_INIT(); /* Initialization of the selected RTOS. Macro is defined by
						the RTOS component. */
#endif
	/*** End of Processor Expert internal initialization. ***/

	/* 하드웨어 모듈별 초기화 (OS 부팅 시퀀스화) */
	System_Init();
	Sensor_Init();
	Motor_Init();
	Timer_Init();

	/* CAN 초기화 */
	FlexCAN0_Init_SDK();

	Comm_Init();
	LED_Init();

	/* 전역 인터럽트 허용: 모든 하드웨어 셋팅이 끝난 후 심장 박동 시작 */
	INT_SYS_EnableIRQGlobal();

	/* 백그라운드 무한 루프 */
	for (;;)
	{
		FMSTR_Poll();
		loop_cnt++;

		/* [ISR Diet] 10ms 주기 제어 로직 실행 */
		if (timer_10ms_flag)
		{
			timer_10ms_flag = false;

			Check_CAN_Status();		   /* CAN 버스 상태 모니터링 */
			Wiper_Diagnostic_Task();   /* 1. 진단: 센서/CAN 건강 체크 */
			Wiper_Process_Task();	   /* 2. 처리: ADC 필터링 + 모드 결정 */
			Wiper_Update_Hardware();   /* 3. 출력: LED + 모터 제어 */
		}

		/* 통신 태스크 (독립적 주기로 관리) */
		Wiper_Comm_Task();

		if (exit_code != 0)
		{
			break;
		}
	}

	/*** Don't write any code pass this line, or it will be deleted during code
	 * generation. ***/
	/*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS
	 * component. DON'T MODIFY THIS CODE!!! ***/
#ifdef PEX_RTOS_START
	PEX_RTOS_START(); /* Startup of the selected RTOS. Macro is defined by the
						 RTOS component. */
#endif
	/*** End of RTOS startup code.  ***/
	/*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
	for (;;)
	{
		if (exit_code != 0)
		{
			break;
		}
	}
	return exit_code;
	/*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* ---------------------------------------------------------------- 하드웨어
 * 초기화 함수 구현 */

void System_Init(void)
{
	/* SCG (System Clock Generator) & PCC (Peripheral Clock Controller)
	 * MCU의 메인 클럭 분배 및 핀(PORT) MUX 설정 */
	(void)CLOCK_SYS_Init(g_clockManConfigsArr,
						 CLOCK_MANAGER_CONFIG_CNT,
						 g_clockManCallbacksArr,
						 CLOCK_MANAGER_CALLBACK_CNT);
	(void)CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
	(void)PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
}

void Sensor_Init(void)
{
	/* ADC (Analog-to-Digital Converter) & eDMA (Direct Memory Access)
	 * 가변저항의 아날로그 값을 디지털로 변환하고, CPU 개입 없이 DMA가 메모리로 배달 */
	(void)EDMA_DRV_Init(&dmaController1_State,
						&dmaController1_InitConfig0,
						edmaChnStateArray,
						edmaChnConfigArray,
						1U);
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
	/* FTM (FlexTimer Module)
	 * 서보모터를 움직이기 위한 50Hz PWM 신호 생성 */
	(void)FTM_DRV_Init(INST_FLEXTIMER_PWM1,
					   &flexTimer_pwm1_InitConfig,
					   &ftmStateStruct);
	(void)FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);
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
	(void)INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, (isr_t *)0);
	INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);
	FMSTR_Init();
}

void LED_Init(void)
{
	/* 1. PORTD 클럭 활성화 */
	PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;

	/* 2. 핀 기능을 GPIO로 설정 (MUX=1) */
	PORTD->PCR[LED_PIN_BLUE] = PORT_PCR_MUX(1);
	PORTD->PCR[LED_PIN_RED] = PORT_PCR_MUX(1);
	PORTD->PCR[LED_PIN_GREEN] = PORT_PCR_MUX(1);

	/* 3. 데이터 방향을 출력(Output)으로 설정 */
	PTD->PDDR |= (1U << LED_PIN_BLUE) | (1U << LED_PIN_RED) | (1U << LED_PIN_GREEN);

	/* 4. 초기 상태: 모든 LED 끄기 (Active Low → High 출력) */
	PTD->PSOR |= (1U << LED_PIN_BLUE) | (1U << LED_PIN_RED) | (1U << LED_PIN_GREEN);
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
	uint16_t delta =
		(current > lastValue) ? (current - lastValue) : (lastValue - current);

	/* 결함 상태에서도 lastValue는 계속 업데이트해줘야 단선 복구 시 오진단을 막음 */
	lastValue = current;

	return (delta <= MAX_DELTA);
}

/* END main */


