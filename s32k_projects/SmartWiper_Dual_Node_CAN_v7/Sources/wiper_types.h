/*
 * [FILE] wiper_types.h
 * [ROLE] Project-wide shared types, constants, and node configuration
 * [VERSION] v7 (Dual-Node Conditional Compilation)
 * [AUTHOR] sisung
 */

#ifndef WIPER_TYPES_H_
#define WIPER_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * NODE SELECTION (Build Configuration)
 * ================================================================
 * Change CURRENT_NODE to NODE_A or NODE_B before building.
 * Node A (Master): ADC sensor + buttons -> CAN command TX
 * Node B (Slave):  Servo motor + buttons -> CAN status TX
 * ================================================================ */
#define NODE_A          0U
#define NODE_B          1U

#define CURRENT_NODE    NODE_B   /* <<< CHANGE HERE TO SWITCH NODE */

/* ================================================================
 * Wiper Mode / Step Enums (shared by both nodes)
 * ================================================================ */

typedef enum
{
	MODE_OFF  = 0U,
	MODE_INT  = 1U,
	MODE_LOW  = 2U,
	MODE_HIGH = 3U
} WiperMode_t;

typedef enum
{
	WIPER_IDLE        = 0U,
	WIPER_MOVING_UP   = 1U,
	WIPER_MOVING_DOWN = 2U
} WiperStep_t;

/* ================================================================
 * Node B Status (Stuck detection)
 * ================================================================ */
typedef enum
{
	STATUS_NORMAL = 0U,
	STATUS_STUCK  = 1U
} NodeB_Status_t;

/* ================================================================
 * Control Mode (Node A: Auto / Manual)
 * ================================================================ */
typedef enum
{
	CTRL_AUTO   = 0U,
	CTRL_MANUAL = 1U
} CtrlMode_t;

/* ================================================================
 * CAN Message Structures (Bidirectional Protocol)
 * ================================================================ */

/* A -> B (ID 0x100): Control command */
typedef struct
{
	uint8_t ctrlMode;       /* CtrlMode_t: AUTO / MANUAL */
	uint8_t wiperCommand;   /* WiperMode_t: OFF/INT/LOW/HIGH (auto) or single trigger flag (manual) */
	uint8_t adcHigh;        /* filteredAdc >> 8 */
	uint8_t adcLow;         /* filteredAdc & 0xFF */
} CanMsg_A2B_t;

/* B -> A (ID 0x200): Status report */
typedef struct
{
	uint8_t currentStep;    /* WiperStep_t: IDLE/UP/DOWN */
	uint8_t status;         /* NodeB_Status_t: NORMAL/STUCK */
	uint8_t reserved0;
	uint8_t reserved1;
} CanMsg_B2A_t;

/* ================================================================
 * CAN IDs (Bidirectional)
 * ================================================================ */
#define CAN_ID_A2B      0x100U  /* Node A -> Node B (control) */
#define CAN_ID_B2A      0x200U  /* Node B -> Node A (status)  */

/* Node-specific TX/RX ID auto-selection */
#if (CURRENT_NODE == NODE_A)
	#define CAN_MY_TX_ID    CAN_ID_A2B
	#define CAN_MY_RX_ID    CAN_ID_B2A
#else
	#define CAN_MY_TX_ID    CAN_ID_B2A
	#define CAN_MY_RX_ID    CAN_ID_A2B
#endif

/* ================================================================
 * Servo Motor PWM Constants (Node B only, but defined globally)
 * ================================================================ */
#define POS_0_DEG       (800U)
#define POS_140_DEG     (3300U)

/* ================================================================
 * Timer and Control Period Constants
 * ================================================================ */
#define INT_WAIT_TIME   (3000U)
#define CAN_TX_PERIOD   (100U)
#define TICK_PERIOD     (10U)
#define CAN_RX_TIMEOUT  (500U)   /* 500ms no-message -> comm loss */

/* ================================================================
 * ADC Filter and Diagnostic Constants (Node A)
 * ================================================================ */
#define FILTER_SIZE     8U
#define FILTER_SHIFT    3U

#define ADC_MIN_SAFE    50U
#define ADC_MAX_SAFE    4040U
#define MAX_DELTA       1500U

#define ADC_THRES_OFF_INT  500U
#define ADC_THRES_INT_LOW  2000U
#define ADC_THRES_LOW_HIGH 3500U

/* ================================================================
 * LED Pin Numbers (PORTD, shared by both nodes)
 * ================================================================ */
#define LED_PIN_BLUE    0U
#define LED_PIN_RED     15U
#define LED_PIN_GREEN   16U

/* ================================================================
 * Button Pin Numbers (PORTC, shared by both nodes)
 * ================================================================ */
#define BTN1_PIN        12U     /* PTC12: SW2 */
#define BTN2_PIN        13U     /* PTC13: SW3 */

#endif /* WIPER_TYPES_H_ */
