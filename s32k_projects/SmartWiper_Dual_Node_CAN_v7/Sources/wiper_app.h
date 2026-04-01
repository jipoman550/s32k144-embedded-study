/*
 * [FILE] wiper_app.h
 * [ROLE] Application Layer interface (Dual-Node)
 * [VERSION] v7 (Dual-Node Conditional Compilation)
 * [AUTHOR] sisung
 */

#ifndef WIPER_APP_H_
#define WIPER_APP_H_

#include "wiper_types.h"

/* ================================================================
 * APP Task Functions (same names, different behavior per node)
 * ================================================================ */
void Wiper_Diagnostic_Task(void);
void Wiper_Process_Task(void);
void Wiper_Update_Hardware(void);
void Wiper_Comm_Task(void);

/* ================================================================
 * FreeMASTER Monitoring Variables (extern)
 * ================================================================ */

#if (CURRENT_NODE == NODE_A)
extern volatile uint16_t filteredAdc;
extern volatile CtrlMode_t currentCtrlMode;
#endif

#if (CURRENT_NODE == NODE_B)
extern volatile NodeB_Status_t nodeB_status;
extern volatile WiperStep_t currentStep_monitor;
#endif

#endif /* WIPER_APP_H_ */
