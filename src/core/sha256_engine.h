/**
 * @file sha256_engine.h
 * @brief High-performance SHA256 engine with midstate optimization
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_SHA256_ENGINE_H
#define PDQ_SHA256_ENGINE_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqSha256Init(PdqSha256Context_t* p_Ctx);
PdqError_t PdqSha256Update(PdqSha256Context_t* p_Ctx, const uint8_t* p_Data, size_t Length);
PdqError_t PdqSha256Final(PdqSha256Context_t* p_Ctx, uint8_t* p_Hash);
PdqError_t PdqSha256(const uint8_t* p_Data, size_t Length, uint8_t* p_Hash);
PdqError_t PdqSha256d(const uint8_t* p_Data, size_t Length, uint8_t* p_Hash);
PdqError_t PdqSha256Midstate(const uint8_t* p_BlockHeader, uint8_t* p_Midstate);
PdqError_t PdqSha256MineBlock(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found);
PdqError_t PdqSha256MineBlockHw(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found);

#ifdef __cplusplus
}
#endif

#endif
