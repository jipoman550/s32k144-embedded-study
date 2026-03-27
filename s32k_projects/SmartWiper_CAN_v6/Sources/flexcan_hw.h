#ifndef FLEXCAN_HW_H_
#define FLEXCAN_HW_H_

#include "S32K144.h"
#include "canCom1.h" // SDK 헤더 이름 확인 (보통 툴에서 생성한 이름)

// 1. 수신된 메시지를 담을 구조체 (데이터, ID, 길이 정보 포함)
/* 42과정 규칙: 헤더 파일에는 'extern'으로 변수의 존재만 알립니다! */
extern flexcan_msgbuff_t rx_msg;
extern volatile uint32_t can_rx_count;
extern volatile uint8_t last_rx_data[4];

/* CAN 초기화: 내부적으로 SDK의 Init 기능을 호출 */
void FlexCAN0_Init_SDK(void);

/* 데이터 송신: 필터링된 ADC값과 현재 모드/스텝을 인자로 받음 */
void FlexCAN0_Send_Wiper_Data(uint16_t adc_val, uint8_t mode, uint8_t step);

#endif
