/*
 * [FILE] flexcan_hw.h
 * [ROLE] MCAL Layer - FlexCAN hardware driver interface
 * [VERSION] v7 (Dual-Node)
 * [AUTHOR] sisung
 */

#ifndef FLEXCAN_HW_H_
#define FLEXCAN_HW_H_

#include "S32K144.h"
#include "canCom1.h"
#include "wiper_types.h"

/* Rx message buffer and status variables */
extern flexcan_msgbuff_t rx_msg;
extern volatile uint32_t can_rx_count;
extern volatile uint8_t last_rx_data[4];

/* [Observability] CAN error monitoring */
extern volatile uint32_t can_tx_err_cnt;
extern volatile uint32_t can_rx_err_cnt;
extern volatile uint32_t can_ack_err_cnt;
extern volatile uint32_t can_last_err_code;
extern volatile bool is_can_failsafe;

/* CAN init and status check */
void FlexCAN0_Init_SDK(void);
void Check_CAN_Status(void);

/* Generic CAN send (node-independent) */
void FlexCAN0_SendMsg(uint32_t msgId, const uint8_t *data, uint8_t len);

/* Legacy send wrapper (kept for compatibility) */
void FlexCAN0_Send_Wiper_Data(uint16_t adc_val, uint8_t mode, uint8_t step);

#endif /* FLEXCAN_HW_H_ */
