/**
 * @file device_api.c
 * @brief REST API for PDQManager communication stub
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "device_api.h"

static bool s_Running = false;

PdqError_t PdqApiInit(void) {
    s_Running = false;
    return PdqOk;
}

PdqError_t PdqApiStart(void) {
    s_Running = true;
    return PdqOk;
}

PdqError_t PdqApiStop(void) {
    s_Running = false;
    return PdqOk;
}

PdqError_t PdqApiProcess(void) {
    return PdqOk;
}
