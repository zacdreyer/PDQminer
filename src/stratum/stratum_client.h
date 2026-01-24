/**
 * @file stratum_client.h
 * @brief Stratum V1 protocol client
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_STRATUM_CLIENT_H
#define PDQ_STRATUM_CLIENT_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqStratumInit(void);
PdqError_t PdqStratumConnect(const char* p_Host, uint16_t Port);
PdqError_t PdqStratumDisconnect(void);
PdqError_t PdqStratumAuthorize(const char* p_Worker, const char* p_Password);
PdqError_t PdqStratumSubscribe(void);
PdqError_t PdqStratumSubmitShare(uint64_t JobId, uint32_t Nonce, uint32_t NTime);
PdqError_t PdqStratumProcess(void);
bool       PdqStratumIsConnected(void);
bool       PdqStratumHasNewJob(void);
PdqError_t PdqStratumGetJob(PdqMiningJob_t* p_Job);

#ifdef __cplusplus
}
#endif

#endif
