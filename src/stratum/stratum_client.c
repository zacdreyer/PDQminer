/**
 * @file stratum_client.c
 * @brief Stratum V1 protocol client stub
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "stratum_client.h"
#include <string.h>

static bool s_Connected = false;
static bool s_HasNewJob = false;
static PdqMiningJob_t s_CurrentJob;

PdqError_t PdqStratumInit(void) {
    s_Connected = false;
    s_HasNewJob = false;
    memset(&s_CurrentJob, 0, sizeof(s_CurrentJob));
    return PdqOk;
}

PdqError_t PdqStratumConnect(const char* p_Host, uint16_t Port) {
    if (p_Host == NULL || Port == 0) return PdqErrorInvalidParam;
    s_Connected = true;
    return PdqOk;
}

PdqError_t PdqStratumDisconnect(void) {
    s_Connected = false;
    return PdqOk;
}

PdqError_t PdqStratumAuthorize(const char* p_Worker, const char* p_Password) {
    if (p_Worker == NULL) return PdqErrorInvalidParam;
    return PdqOk;
}

PdqError_t PdqStratumSubscribe(void) {
    return PdqOk;
}

PdqError_t PdqStratumSubmitShare(uint64_t JobId, uint32_t Nonce, uint32_t NTime) {
    return PdqOk;
}

PdqError_t PdqStratumProcess(void) {
    return PdqOk;
}

bool PdqStratumIsConnected(void) {
    return s_Connected;
}

bool PdqStratumHasNewJob(void) {
    bool Has = s_HasNewJob;
    s_HasNewJob = false;
    return Has;
}

PdqError_t PdqStratumGetJob(PdqMiningJob_t* p_Job) {
    if (p_Job == NULL) return PdqErrorInvalidParam;
    memcpy(p_Job, &s_CurrentJob, sizeof(PdqMiningJob_t));
    return PdqOk;
}
