#include "flexcan_hw.h"

flexcan_state_t canState0; // t_list처럼 CAN의 상태를 관리하는 전역 구조체

/* 실제 변수 메모리 할당은 .c 파일에서 합니다. */
flexcan_msgbuff_t rx_msg;
volatile uint32_t can_rx_count = 0U;
volatile uint8_t last_rx_data[4] = {0U, 0U, 0U, 0U};

/* [Observability] 실제 변수 할당 및 초기화 */
volatile uint32_t can_tx_err_cnt = 0U;
volatile uint32_t can_rx_err_cnt = 0U;
volatile uint32_t can_ack_err_cnt = 0U;
volatile uint32_t can_last_err_code = 0U;
volatile bool is_can_failsafe = false;

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
    FLEXCAN_DRV_ConfigRxMb(INST_CANCOM1, 1U, &rx_info, CAN_WIPER_RX_ID);

    /* 비동기 수신 시작 */
    // "1번 MB에 ID 0x100인 데이터가 오면 rx_msg 변수에 써라"는 명령입니다.
    FLEXCAN_DRV_Receive(INST_CANCOM1, 1U, &rx_msg);
}

void Check_CAN_Status(void)
{
    uint32_t esr1_val = CAN0->ESR1; // ESR1 레지스터 직접 읽기
    can_last_err_code = esr1_val;  // 전체 레지스터 값 보존

    /* 1. ACK 에러 체크 (전송 실패의 주원인) */
    if (esr1_val & CAN_ESR1_ACKERR_MASK)
    {
        can_ack_err_cnt++;
        /* ACK 에러 발생 시 플래그 클리어 (Write 1 to clear) */
        CAN0->ESR1 |= CAN_ESR1_ACKERR_MASK;
    }

    /* 2. 전송/수신 에러 카운터 경고 체크 (TXWRN, RXWRN) */
    if (esr1_val & CAN_ESR1_TXWRN_MASK)
	{
		can_tx_err_cnt++;
	}
    if (esr1_val & CAN_ESR1_RXWRN_MASK)
	{
		can_rx_err_cnt++;
	}

    /* 3. Failsafe 발동 로직 (예: ACK 에러가 연속 10번 이상 발생 시) */
    if (can_ack_err_cnt > 10U)
    {
        is_can_failsafe = true;
    }
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

    FLEXCAN_DRV_Send(INST_CANCOM1, 0U, &dataInfo, CAN_WIPER_TX_ID, canData);
}

