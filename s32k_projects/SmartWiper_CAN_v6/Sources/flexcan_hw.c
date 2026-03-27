#include "flexcan_hw.h"

flexcan_state_t canState0; // t_list처럼 CAN의 상태를 관리하는 전역 구조체

/* 실제 변수 메모리 할당은 .c 파일에서 합니다. */
flexcan_msgbuff_t rx_msg;
volatile uint32_t can_rx_count = 0U;
volatile uint8_t last_rx_data[4] = {0U, 0U, 0U, 0U};

void FlexCAN0_Init_SDK(void)
{
    /* SDK 초기화 (PCC, MCR, Timing 설정 자동 처리) */
    FLEXCAN_DRV_Init(INST_CANCOM1, &canState0, &canCom1_InitConfig0);

    /* 수신용 데이터 인포 구조체 생성 */
    flexcan_data_info_t rx_info =
    {
    		.msg_id_type = FLEXCAN_MSG_ID_STD,
			.data_length = 4U, // 우리가 받을 데이터 4바이트
			.fd_enable = false
    };

    /* 1번 메시지 버퍼(MB)를 수신용으로 설정 */
    // 42과정의 open() 후 대기하는 것과 같습니다.
    FLEXCAN_DRV_ConfigRxMb(INST_CANCOM1, 1U, &rx_info, 0x100);

    /* 비동기 수신 시작 */
    // "1번 MB에 ID 0x100인 데이터가 오면 rx_msg 변수에 써라"는 명령입니다.
    FLEXCAN_DRV_Receive(INST_CANCOM1, 1U, &rx_msg);
}

void FlexCAN0_Send_Wiper_Data(uint16_t adc_val, uint8_t mode, uint8_t step)
{
    flexcan_data_info_t dataInfo =
    {
    		.msg_id_type = FLEXCAN_MSG_ID_STD,
    		.data_length = 4U, // ADC(2B) + Mode(1B) + Step(1B) = 총 4바이트
			.fd_enable = false
    };

    uint8_t canData[4] = {0U, 0U, 0U, 0U};
    /* [비트 연산 상세] 16비트 ADC값을 8비트 2개로 쪼개기 */
    canData[0] = (uint8_t)(adc_val >> 8U);   // 상위 바이트 (MSB)
    canData[1] = (uint8_t)(adc_val & 0xFFU); // 하위 바이트 (LSB)
    canData[2] = mode;
    canData[3] = step;

    /* 0번 메시지 버퍼(MB)를 통해 ID 0x100으로 전송 */
    FLEXCAN_DRV_Send(INST_CANCOM1, 0U, &dataInfo, 0x100, canData);
}
