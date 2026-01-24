/**
 * @file device_api.h
 * @brief REST API for PDQManager communication
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_DEVICE_API_H
#define PDQ_DEVICE_API_H

#include "pdq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

PdqError_t PdqApiInit(void);
PdqError_t PdqApiStart(void);
PdqError_t PdqApiStop(void);
PdqError_t PdqApiProcess(void);

#ifdef __cplusplus
}
#endif

#endif
