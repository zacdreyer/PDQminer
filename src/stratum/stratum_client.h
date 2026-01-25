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

#define PDQ_STRATUM_MAX_JOBID_LEN       64
#define PDQ_STRATUM_MAX_EXTRANONCE_LEN  8
#define PDQ_STRATUM_MAX_COINBASE_LEN    256
#define PDQ_STRATUM_MAX_MERKLE_BRANCHES 16
#define PDQ_STRATUM_RECV_BUFFER_SIZE    4096
#define PDQ_STRATUM_SEND_BUFFER_SIZE    512
#define PDQ_STRATUM_DEFAULT_TIMEOUT_MS  30000

typedef struct {
    char     JobId[PDQ_STRATUM_MAX_JOBID_LEN + 1];
    uint8_t  PrevBlockHash[32];
    uint8_t  Coinbase1[PDQ_STRATUM_MAX_COINBASE_LEN];
    uint16_t Coinbase1Len;
    uint8_t  Coinbase2[PDQ_STRATUM_MAX_COINBASE_LEN];
    uint16_t Coinbase2Len;
    uint8_t  MerkleBranches[PDQ_STRATUM_MAX_MERKLE_BRANCHES][32];
    uint8_t  MerkleBranchCount;
    uint32_t Version;
    uint32_t NBits;
    uint32_t NTime;
    bool     CleanJobs;
} PdqStratumJob_t;

typedef enum {
    StratumStateDisconnected = 0,
    StratumStateConnecting,
    StratumStateConnected,
    StratumStateSubscribing,
    StratumStateSubscribed,
    StratumStateAuthorizing,
    StratumStateAuthorized,
    StratumStateReady
} PdqStratumState_t;

PdqError_t PdqStratumInit(void);
PdqError_t PdqStratumConnect(const char* p_Host, uint16_t Port);
PdqError_t PdqStratumDisconnect(void);
PdqError_t PdqStratumSubscribe(void);
PdqError_t PdqStratumAuthorize(const char* p_Worker, const char* p_Password);
PdqError_t PdqStratumSubmitShare(const char* p_JobId, uint32_t Extranonce2, uint32_t Nonce, uint32_t NTime);
PdqError_t PdqStratumProcess(void);

bool              PdqStratumIsConnected(void);
bool              PdqStratumIsReady(void);
bool              PdqStratumHasNewJob(void);
PdqStratumState_t PdqStratumGetState(void);
PdqError_t        PdqStratumGetJob(PdqStratumJob_t* p_Job);
uint32_t          PdqStratumGetDifficulty(void);
void              PdqStratumGetExtranonce(uint8_t* p_Buffer, uint8_t* p_Len);
uint8_t           PdqStratumGetExtranonce2Size(void);
PdqError_t        PdqStratumBuildMiningJob(const PdqStratumJob_t* p_StratumJob,
                                           const uint8_t* p_Extranonce1, uint8_t Extranonce1Len,
                                           uint32_t Extranonce2, uint8_t Extranonce2Len,
                                           uint32_t Difficulty,
                                           PdqMiningJob_t* p_MiningJob);

#ifdef __cplusplus
}
#endif

#endif