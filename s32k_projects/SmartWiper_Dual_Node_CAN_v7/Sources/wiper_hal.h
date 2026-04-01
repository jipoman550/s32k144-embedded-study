/*
 * [FILE] wiper_hal.h
 * [ROLE] Hardware Abstraction Layer interface (Dual-Node)
 * [VERSION] v7 (Dual-Node Conditional Compilation)
 * [AUTHOR] sisung
 */

#ifndef WIPER_HAL_H_
#define WIPER_HAL_H_

#include "wiper_types.h"

/* ================================================================
 * System Init
 * ================================================================ */
void HAL_Wiper_InitAll(void);

/* ================================================================
 * Sensor Interface (ADC) - Node A only
 * ================================================================ */
uint16_t HAL_Wiper_GetRawAdc(void);

/* ================================================================
 * Actuator Interface (Servo PWM) - Node B only
 * ================================================================ */
void HAL_Wiper_SetServoDuty(uint32_t duty);

/* ================================================================
 * LED Interface (both nodes)
 * ================================================================ */
void HAL_Wiper_AllLedsOff(void);
void HAL_Wiper_SetLedOn(uint32_t pin);

/* ================================================================
 * Button Interface (both nodes, different roles)
 * Node A: BTN1=Auto/Manual toggle, BTN2=Single trigger
 * Node B: BTN1=Stuck simulate, BTN2=Error clear
 * ================================================================ */
bool HAL_Wiper_IsBtn1Pressed(void);
bool HAL_Wiper_IsBtn2Pressed(void);

/* ================================================================
 * CAN Communication Interface
 * ================================================================ */
void HAL_Wiper_CheckComm(void);
bool HAL_Wiper_IsCommFault(void);
void HAL_Wiper_ResetCommFault(void);

/** @brief Send raw CAN message using MY_TX_ID */
void HAL_Wiper_SendMsg(const uint8_t *data, uint8_t len);

/** @brief Poll CAN RX and copy data to last_rx_data */
void HAL_Wiper_PollRx(void);

/** @brief Get pointer to last received data */
const volatile uint8_t* HAL_Wiper_GetRxData(void);

/** @brief Check if new CAN data was received since last call */
bool HAL_Wiper_HasNewRxData(void);

/* ================================================================
 * Timing Interface
 * ================================================================ */
uint32_t HAL_Wiper_GetTicks(void);
void HAL_Wiper_PollFreemaster(void);

/* ================================================================
 * ISR Flag (extern)
 * ================================================================ */
extern volatile bool timer_10ms_flag;

#endif /* WIPER_HAL_H_ */
