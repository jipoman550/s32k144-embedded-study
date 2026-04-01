/*
 * [FILE] wiper_hal.c
 * [ROLE] Hardware Abstraction Layer (Dual-Node)
 * [VERSION] v7 (Dual-Node Conditional Compilation)
 * [AUTHOR] sisung
 */

/* ---------------------------------------------------------------- SDK */
#include "Cpu.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "flexcan_hw.h"
#include "freemaster.h"
#include "lpit1.h"
#include "osif.h"

#if (CURRENT_NODE == NODE_A)
#include "adConv1.h"
#include "dmaController1.h"
#endif

#if (CURRENT_NODE == NODE_B)
#include "flexTimer_pwm1.h"
#endif

/* ---------------------------------------------------------------- Self */
#include "wiper_hal.h"

/* ================================================================
 * Module-scope variables
 * ================================================================ */

#if (CURRENT_NODE == NODE_B)
static ftm_state_t ftmStateStruct;
#endif

#if (CURRENT_NODE == NODE_A)
volatile uint16_t adcValue = 0U;
#endif

/* System timing */
volatile uint32_t ms_ticks = 0U;
volatile uint32_t loop_cnt = 0U;
volatile bool timer_10ms_flag = false;

/* FreeMASTER control */
volatile uint8_t errorResetRequest = 0U;
volatile uint8_t controlSource = 0U;
volatile WiperMode_t pcModeRequest = MODE_OFF;

/* CAN RX tracking */
static volatile bool newRxFlag = false;
static volatile uint32_t lastRxTick = 0U;

/* Button debounce */
static bool btn1_prev = false;
static bool btn2_prev = false;

/* ================================================================
 * ISR (shared by both nodes)
 * ================================================================ */
void LPIT0_Ch0_IRQHandler(void)
{
	LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1U << 0U));
	ms_ticks += TICK_PERIOD;
	timer_10ms_flag = true;
}

/* ================================================================
 * Private Init Functions
 * ================================================================ */

static void HAL_InitSystem(void)
{
	(void)CLOCK_SYS_Init(g_clockManConfigsArr,
						 CLOCK_MANAGER_CONFIG_CNT,
						 g_clockManCallbacksArr,
						 CLOCK_MANAGER_CALLBACK_CNT);
	(void)CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
	(void)PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
}

#if (CURRENT_NODE == NODE_A)
static void HAL_InitSensor(void)
{
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
#endif

#if (CURRENT_NODE == NODE_B)
static void HAL_InitMotor(void)
{
	(void)FTM_DRV_Init(INST_FLEXTIMER_PWM1,
					   &flexTimer_pwm1_InitConfig,
					   &ftmStateStruct);
	(void)FTM_DRV_InitPwm(INST_FLEXTIMER_PWM1, &flexTimer_pwm1_PwmConfig);
}
#endif

static void HAL_InitTimer(void)
{
	(void)LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
	(void)LPIT_DRV_InitChannel(INST_LPIT1, 0, &lpit1_ChnConfig0);
	LPIT_DRV_StartTimerChannels(INST_LPIT1, (1U << 0U));
}

static void HAL_InitComm(void)
{
	(void)LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);
	(void)INT_SYS_InstallHandler(LPUART1_RxTx_IRQn, FMSTR_Isr, (isr_t *)0);
	INT_SYS_EnableIRQ(LPUART1_RxTx_IRQn);
	FMSTR_Init();
}

static void HAL_InitLed(void)
{
	PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;

	PORTD->PCR[LED_PIN_BLUE]  = PORT_PCR_MUX(1);
	PORTD->PCR[LED_PIN_RED]   = PORT_PCR_MUX(1);
	PORTD->PCR[LED_PIN_GREEN] = PORT_PCR_MUX(1);

	PTD->PDDR |= (1U << LED_PIN_BLUE) | (1U << LED_PIN_RED) | (1U << LED_PIN_GREEN);
	PTD->PSOR |= (1U << LED_PIN_BLUE) | (1U << LED_PIN_RED) | (1U << LED_PIN_GREEN);
}

static void HAL_InitButton(void)
{
	/* PORTC clock enable */
	PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;

	/* PTC12, PTC13: GPIO input with internal pull-up */
	PORTC->PCR[BTN1_PIN] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
	PORTC->PCR[BTN2_PIN] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

	/* Direction: Input (clear PDDR bits) */
	PTC->PDDR &= ~((1U << BTN1_PIN) | (1U << BTN2_PIN));
}

/* ================================================================
 * Public Interface Implementation
 * ================================================================ */

void HAL_Wiper_InitAll(void)
{
	HAL_InitSystem();

#if (CURRENT_NODE == NODE_A)
	HAL_InitSensor();    /* ADC + DMA (Node A only) */
#endif

#if (CURRENT_NODE == NODE_B)
	HAL_InitMotor();     /* FTM/PWM (Node B only) */
#endif

	HAL_InitTimer();
	FlexCAN0_Init_SDK();
	HAL_InitComm();
	HAL_InitLed();
	HAL_InitButton();
	INT_SYS_EnableIRQGlobal();
}

/* ---- Sensor (Node A) ---- */
uint16_t HAL_Wiper_GetRawAdc(void)
{
#if (CURRENT_NODE == NODE_A)
	return adcValue;
#else
	return 0U; /* Node B has no ADC */
#endif
}

/* ---- Actuator (Node B) ---- */
void HAL_Wiper_SetServoDuty(uint32_t duty)
{
#if (CURRENT_NODE == NODE_B)
	(void)FTM_DRV_UpdatePwmChannel(INST_FLEXTIMER_PWM1, 0U,
								   FTM_PWM_UPDATE_IN_DUTY_CYCLE,
								   duty, 0U, true);
#else
	(void)duty; /* Node A has no servo */
#endif
}

/* ---- LED ---- */
void HAL_Wiper_AllLedsOff(void)
{
	PTD->PSOR |= (1U << LED_PIN_BLUE) | (1U << LED_PIN_RED) | (1U << LED_PIN_GREEN);
}

void HAL_Wiper_SetLedOn(uint32_t pin)
{
	PTD->PCOR |= (1U << pin);
}

/* ---- Button (rising-edge detect with debounce) ---- */
bool HAL_Wiper_IsBtn1Pressed(void)
{
	bool raw = ((PTC->PDIR & (1U << BTN1_PIN)) == 0U); /* Active Low */
	bool rising = (raw && !btn1_prev);
	btn1_prev = raw;
	return rising;
}

bool HAL_Wiper_IsBtn2Pressed(void)
{
	bool raw = ((PTC->PDIR & (1U << BTN2_PIN)) == 0U);
	bool rising = (raw && !btn2_prev);
	btn2_prev = raw;
	return rising;
}

/* ---- CAN Communication ---- */
void HAL_Wiper_CheckComm(void)
{
	Check_CAN_Status();
}

bool HAL_Wiper_IsCommFault(void)
{
	return is_can_failsafe;
}

void HAL_Wiper_ResetCommFault(void)
{
	is_can_failsafe = false;
	can_ack_err_cnt = 0U;
	errorResetRequest = 0U;
}

void HAL_Wiper_SendMsg(const uint8_t *data, uint8_t len)
{
	FlexCAN0_SendMsg(CAN_MY_TX_ID, data, len);
}

void HAL_Wiper_PollRx(void)
{
	if (FLEXCAN_DRV_GetTransferStatus(INST_CANCOM1, 1U) == STATUS_SUCCESS)
	{
		can_rx_count++;
		lastRxTick = ms_ticks;
		newRxFlag = true;

		last_rx_data[0] = rx_msg.data[0];
		last_rx_data[1] = rx_msg.data[1];
		last_rx_data[2] = rx_msg.data[2];
		last_rx_data[3] = rx_msg.data[3];

		FLEXCAN_DRV_Receive(INST_CANCOM1, 1U, &rx_msg);
	}

	/* Communication timeout detection */
	if ((can_rx_count > 0U) && ((ms_ticks - lastRxTick) > CAN_RX_TIMEOUT))
	{
		is_can_failsafe = true;
	}
}

const volatile uint8_t* HAL_Wiper_GetRxData(void)
{
	return last_rx_data;
}

bool HAL_Wiper_HasNewRxData(void)
{
	if (newRxFlag)
	{
		newRxFlag = false;
		return true;
	}
	return false;
}

/* ---- Timing ---- */
uint32_t HAL_Wiper_GetTicks(void)
{
	return ms_ticks;
}

void HAL_Wiper_PollFreemaster(void)
{
	FMSTR_Poll();
	loop_cnt++;
}
