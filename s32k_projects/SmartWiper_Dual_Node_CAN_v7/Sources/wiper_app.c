/*
 * [FILE] wiper_app.c
 * [ROLE] Application Layer - Dual-Node (Node A: Master, Node B: Slave)
 * [VERSION] v7 (Dual-Node Conditional Compilation)
 * [AUTHOR] sisung
 *
 * Node A (Master): ADC read -> filter -> mode decide -> CAN TX command
 * Node B (Slave):  CAN RX command -> servo state machine -> CAN TX status
 */

#include "wiper_app.h"
#include "wiper_hal.h"

/* ================================================================
 *                    SHARED VARIABLES
 * ================================================================ */
static WiperMode_t currentMode = MODE_OFF;
static bool is_fault = false;
static uint32_t lastCanTxTime = 0U;

/* ================================================================
 *                    NODE A (MASTER) CODE
 * ================================================================ */
#if (CURRENT_NODE == NODE_A)

/* --- Node A state --- */
volatile uint16_t filteredAdc = 0U;
volatile CtrlMode_t currentCtrlMode = CTRL_AUTO;

static uint16_t adcBuffer[FILTER_SIZE] = {0U};
static uint32_t adcSum = 0U;
static uint8_t  bufferIdx = 0U;
static uint16_t lastValue = 0U;

/* Single-trigger request (manual mode) */
static bool singleTriggerReq = false;

/* Node B feedback (received via CAN) */
static WiperStep_t remoteStep = WIPER_IDLE;
static NodeB_Status_t remoteStatus = STATUS_NORMAL;

/* --- Node A Diagnostic --- */
static bool Is_Sensor_Healthy(uint16_t value)
{
	if ((value < ADC_MIN_SAFE) || (value > ADC_MAX_SAFE))
	{
		return false;
	}
	return true;
}

static bool Is_Signal_Stable(uint16_t current)
{
	uint16_t delta =
		(current > lastValue) ? (current - lastValue) : (lastValue - current);
	lastValue = current;
	return (delta <= MAX_DELTA);
}

void Wiper_Diagnostic_Task(void)
{
	uint16_t rawAdc = HAL_Wiper_GetRawAdc();
	bool healthy = Is_Sensor_Healthy(rawAdc);
	bool stable  = Is_Signal_Stable(rawAdc);

	is_fault = (!healthy || !stable || HAL_Wiper_IsCommFault());

	/* If Node B reports STUCK, treat as fault */
	if (remoteStatus == STATUS_STUCK)
	{
		is_fault = true;
	}

	if (is_fault)
	{
		currentMode = MODE_OFF;
	}
}

/* --- Node A Process: ADC filter + mode decision + button handling --- */
void Wiper_Process_Task(void)
{
	/* Button 1: Auto/Manual toggle */
	if (HAL_Wiper_IsBtn1Pressed())
	{
		currentCtrlMode = (currentCtrlMode == CTRL_AUTO) ? CTRL_MANUAL : CTRL_AUTO;
		if (currentCtrlMode == CTRL_MANUAL)
		{
			currentMode = MODE_OFF;
		}
	}

	/* Button 2: Single trigger (manual mode only) */
	if (HAL_Wiper_IsBtn2Pressed())
	{
		if (currentCtrlMode == CTRL_MANUAL)
		{
			singleTriggerReq = true;
		}
	}

	if (!is_fault)
	{
		uint16_t rawAdc = HAL_Wiper_GetRawAdc();

		/* Moving average filter */
		adcSum -= adcBuffer[bufferIdx];
		adcBuffer[bufferIdx] = rawAdc;
		adcSum += adcBuffer[bufferIdx];
		bufferIdx = (uint8_t)((bufferIdx + 1U) % FILTER_SIZE);
		filteredAdc = (uint16_t)(adcSum >> FILTER_SHIFT);

		/* Mode decision (auto mode only) */
		if (currentCtrlMode == CTRL_AUTO)
		{
			extern volatile uint8_t controlSource;
			extern volatile WiperMode_t pcModeRequest;

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
		/* Manual mode: mode stays OFF unless single trigger */
	}
}

/* --- Node A Hardware: LED only (servo is on Node B) --- */
void Wiper_Update_Hardware(void)
{
	HAL_Wiper_AllLedsOff();

	if (HAL_Wiper_IsCommFault() || (remoteStatus == STATUS_STUCK))
	{
		/* Error: red LED blink 500ms */
		if ((HAL_Wiper_GetTicks() % 1000U) < 500U)
		{
			HAL_Wiper_SetLedOn(LED_PIN_RED);
		}
	}
	else
	{
		if (currentMode == MODE_OFF)
		{
			/* All off */
		}
		else if (currentMode == MODE_INT)
		{
			HAL_Wiper_SetLedOn(LED_PIN_BLUE);
		}
		else if (currentMode == MODE_LOW)
		{
			HAL_Wiper_SetLedOn(LED_PIN_GREEN);
		}
		else if (currentMode == MODE_HIGH)
		{
			HAL_Wiper_SetLedOn(LED_PIN_RED);
		}
	}
}

/* --- Node A Comm: TX command (A2B), RX status (B2A) --- */
void Wiper_Comm_Task(void)
{
	uint32_t now = HAL_Wiper_GetTicks();

	/* TX: send command every 100ms */
	if ((now - lastCanTxTime) >= CAN_TX_PERIOD)
	{
		/* Recovery: manual reset */
		extern volatile uint8_t errorResetRequest;
		if (HAL_Wiper_IsCommFault())
		{
			if (errorResetRequest == 1U)
			{
				HAL_Wiper_ResetCommFault();
			}
		}

		CanMsg_A2B_t txMsg;
		txMsg.ctrlMode = (uint8_t)currentCtrlMode;

		if ((currentCtrlMode == CTRL_MANUAL) && singleTriggerReq)
		{
			txMsg.wiperCommand = 0xFFU; /* Special: single trigger */
			singleTriggerReq = false;
		}
		else
		{
			txMsg.wiperCommand = (uint8_t)currentMode;
		}

		txMsg.adcHigh = (uint8_t)(filteredAdc >> 8U);
		txMsg.adcLow  = (uint8_t)(filteredAdc & 0xFFU);

		HAL_Wiper_SendMsg((const uint8_t *)&txMsg, 4U);
		lastCanTxTime = now;
	}

	/* RX: receive Node B status */
	HAL_Wiper_PollRx();

	if (HAL_Wiper_HasNewRxData())
	{
		const volatile uint8_t *rxData = HAL_Wiper_GetRxData();
		remoteStep   = (WiperStep_t)rxData[0];
		remoteStatus = (NodeB_Status_t)rxData[1];
	}
}

#endif /* NODE_A */

/* ================================================================
 *                    NODE B (SLAVE) CODE
 * ================================================================ */
#if (CURRENT_NODE == NODE_B)

/* --- Node B state --- */
volatile NodeB_Status_t nodeB_status = STATUS_NORMAL;
volatile WiperStep_t currentStep_monitor = WIPER_IDLE;

static WiperStep_t currentStep = WIPER_IDLE;
static uint32_t stepCounter = 0U;

/* Received command from Node A */
static CtrlMode_t rxCtrlMode = CTRL_AUTO;
static uint8_t rxWiperCommand = 0U;
static bool singleTriggerActive = false;
static bool singleCycleDone = false;

/* --- Node B Diagnostic: CAN timeout + Stuck button --- */
void Wiper_Diagnostic_Task(void)
{
	/* BTN1 = Stuck simulate (press to simulate mechanical jam) */
	if (HAL_Wiper_IsBtn1Pressed())
	{
		nodeB_status = STATUS_STUCK;
	}

	/* BTN2 = Error clear (press to recover from stuck) */
	if (HAL_Wiper_IsBtn2Pressed())
	{
		nodeB_status = STATUS_NORMAL;
		if (HAL_Wiper_IsCommFault())
		{
			HAL_Wiper_ResetCommFault();
		}
	}

	is_fault = (HAL_Wiper_IsCommFault() || (nodeB_status == STATUS_STUCK));

	if (is_fault)
	{
		currentMode = MODE_OFF;
	}
}

/* --- Node B Process: interpret received CAN command --- */
void Wiper_Process_Task(void)
{
	HAL_Wiper_PollRx();

	if (HAL_Wiper_HasNewRxData())
	{
		const volatile uint8_t *rxData = HAL_Wiper_GetRxData();
		rxCtrlMode    = (CtrlMode_t)rxData[0];
		rxWiperCommand = rxData[1];
	}

	if (!is_fault)
	{
		if (rxCtrlMode == CTRL_AUTO)
		{
			currentMode = (WiperMode_t)rxWiperCommand;
			singleTriggerActive = false;
			singleCycleDone = false;
		}
		else /* CTRL_MANUAL */
		{
			if (rxWiperCommand == 0xFFU) /* Single trigger received */
			{
				if (!singleTriggerActive)
				{
					singleTriggerActive = true;
					singleCycleDone = false;
					currentMode = MODE_LOW; /* Single = one LOW-speed cycle */
				}
			}
			else
			{
				if (!singleTriggerActive)
				{
					currentMode = MODE_OFF;
				}
			}
		}
	}
}

/* --- Node B Hardware: servo state machine + LED --- */
void Wiper_Update_Hardware(void)
{
	/* === LED feedback === */
	HAL_Wiper_AllLedsOff();

	if (nodeB_status == STATUS_STUCK)
	{
		/* Stuck: red+blue alternate fast (250ms) */
		if ((HAL_Wiper_GetTicks() % 500U) < 250U)
		{
			HAL_Wiper_SetLedOn(LED_PIN_RED);
		}
		else
		{
			HAL_Wiper_SetLedOn(LED_PIN_BLUE);
		}
	}
	else if (HAL_Wiper_IsCommFault())
	{
		if ((HAL_Wiper_GetTicks() % 1000U) < 500U)
		{
			HAL_Wiper_SetLedOn(LED_PIN_RED);
		}
	}
	else
	{
		if (currentStep == WIPER_IDLE)
		{
			HAL_Wiper_SetLedOn(LED_PIN_GREEN);
		}
		else
		{
			HAL_Wiper_SetLedOn(LED_PIN_BLUE);
		}
	}

	/* === Servo state machine === */
	if (nodeB_status == STATUS_STUCK)
	{
		/* STUCK: immediate stop at safe position */
		HAL_Wiper_SetServoDuty(POS_0_DEG);
		currentStep = WIPER_IDLE;
		stepCounter = 0U;
		currentStep_monitor = currentStep;
		return;
	}

	uint32_t moveDuration = 0U;

	if (currentMode == MODE_OFF)
	{
		HAL_Wiper_SetServoDuty(POS_0_DEG);
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
			HAL_Wiper_SetServoDuty(POS_140_DEG);
		}

		stepCounter += TICK_PERIOD;

		if (currentStep == WIPER_MOVING_UP)
		{
			if (stepCounter >= moveDuration)
			{
				currentStep = WIPER_MOVING_DOWN;
				stepCounter = 0U;
				HAL_Wiper_SetServoDuty(POS_0_DEG);
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

				/* Single trigger: one cycle done */
				if (singleTriggerActive)
				{
					singleTriggerActive = false;
					singleCycleDone = true;
					currentMode = MODE_OFF;
				}
			}
		}
		else
		{
			currentStep = WIPER_IDLE;
		}
	}

	currentStep_monitor = currentStep;
}

/* --- Node B Comm: TX status (B2A) --- */
void Wiper_Comm_Task(void)
{
	uint32_t now = HAL_Wiper_GetTicks();

	if ((now - lastCanTxTime) >= CAN_TX_PERIOD)
	{
		CanMsg_B2A_t txMsg;
		txMsg.currentStep = (uint8_t)currentStep;
		txMsg.status      = (uint8_t)nodeB_status;
		txMsg.reserved0   = 0U;
		txMsg.reserved1   = 0U;

		HAL_Wiper_SendMsg((const uint8_t *)&txMsg, 4U);
		lastCanTxTime = now;
	}

	/* Note: RX polling is done in Process_Task for Node B */
}

#endif /* NODE_B */
