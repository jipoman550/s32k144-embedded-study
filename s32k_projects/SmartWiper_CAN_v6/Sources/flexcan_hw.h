#ifndef FLEXCAN_HW_H_
#define FLEXCAN_HW_H_

#include "S32K144.h"
#include "canCom1.h"

/* [MISRA-C] CAN 통신용 ID 정의 (매직 넘버 제거) */
#define CAN_WIPER_TX_ID 0x100U
#define CAN_WIPER_RX_ID 0x100U

// 1. 수신된 메시지를 담을 구조체 및 상태 변수
extern flexcan_msgbuff_t rx_msg;
extern volatile uint32_t can_rx_count;
extern volatile uint8_t last_rx_data[4];

/* [Observability] CAN 통계 및 에러 모니터링용 변수 */
extern volatile uint32_t can_tx_err_cnt;
extern volatile uint32_t can_rx_err_cnt;
extern volatile uint32_t can_ack_err_cnt;
extern volatile uint32_t can_last_err_code;
extern volatile bool is_can_failsafe;

/* CAN 초기화 및 상태 체크 함수 */
void FlexCAN0_Init_SDK(void);
void Check_CAN_Status(void);

/* 데이터 송신 함수 */
void FlexCAN0_Send_Wiper_Data(uint16_t adc_val, uint8_t mode, uint8_t step);

#endif

