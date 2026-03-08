/**
 * @file mining_task.h
 * @brief Dual-core mining task management
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_MINING_TASK_H
#define PDQ_MINING_TASK_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqMiningInit(void);
PdqError_t PdqMiningStart(void);
PdqError_t PdqMiningStop(void);
PdqError_t PdqMiningSetJob(const PdqMiningJob_t* p_Job);
PdqError_t PdqMiningGetStats(PdqMinerStats_t* p_Stats);
bool       PdqMiningIsRunning(void);
bool       PdqMiningHasShare(void);
PdqError_t PdqMiningGetShare(PdqShareInfo_t* p_Share);
void       PdqMiningClearShares(void);

#ifdef __cplusplus
}
#endif

#endif