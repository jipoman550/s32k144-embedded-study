#include "flexcan_hw.h"

flexcan_state_t canState0;

/* Rx message buffer */
flexcan_msgbuff_t rx_msg;
volatile uint32_t can_rx_count = 0U;
volatile uint8_t last_rx_data[4] = {0U, 0U, 0U, 0U};

/* [Observability] Error monitoring */
volatile uint32_t can_tx_err_cnt = 0U;
volatile uint32_t can_rx_err_cnt = 0U;
volatile uint32_t can_ack_err_cnt = 0U;
volatile uint32_t can_last_err_code = 0U;
volatile bool is_can_failsafe = false;

void FlexCAN0_Init_SDK(void)
{
    FLEXCAN_DRV_Init(INST_CANCOM1, &canState0, &canCom1_InitConfig0);

    flexcan_data_info_t rx_info =
    {
        .msg_id_type = FLEXCAN_MSG_ID_STD,
        .data_length = 4U,
        .fd_enable = false
    };

    /* RX MB: filter by MY_RX_ID (node-dependent) */
    FLEXCAN_DRV_ConfigRxMb(INST_CANCOM1, 1U, &rx_info, CAN_MY_RX_ID);
    FLEXCAN_DRV_Receive(INST_CANCOM1, 1U, &rx_msg);
}

void Check_CAN_Status(void)
{
    uint32_t esr1_val = CAN0->ESR1;
    can_last_err_code = esr1_val;

    if (esr1_val & CAN_ESR1_ACKERR_MASK)
    {
        can_ack_err_cnt++;
        CAN0->ESR1 |= CAN_ESR1_ACKERR_MASK;
    }

    if (esr1_val & CAN_ESR1_TXWRN_MASK)
    {
        can_tx_err_cnt++;
    }
    if (esr1_val & CAN_ESR1_RXWRN_MASK)
    {
        can_rx_err_cnt++;
    }

    if (can_ack_err_cnt > 10U)
    {
        is_can_failsafe = true;
    }
}

/* Generic CAN send: any ID, any data, any length */
void FlexCAN0_SendMsg(uint32_t msgId, const uint8_t *data, uint8_t len)
{
    flexcan_data_info_t dataInfo =
    {
        .msg_id_type = FLEXCAN_MSG_ID_STD,
        .data_length = len,
        .fd_enable = false
    };

    FLEXCAN_DRV_Send(INST_CANCOM1, 0U, &dataInfo, msgId, (uint8_t *)data);
}

/* Legacy wrapper (kept for backward compatibility) */
void FlexCAN0_Send_Wiper_Data(uint16_t adc_val, uint8_t mode, uint8_t step)
{
    uint8_t canData[4];
    canData[0] = (uint8_t)(adc_val >> 8U);
    canData[1] = (uint8_t)(adc_val & 0xFFU);
    canData[2] = mode;
    canData[3] = step;

    FlexCAN0_SendMsg(CAN_MY_TX_ID, canData, 4U);
}
