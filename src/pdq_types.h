/**
 * @file pdq_types.h
 * @brief Common type definitions for PDQminer
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#ifndef PDQ_TYPES_H
#define PDQ_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PDQ_VERSION_MAJOR 0
#define PDQ_VERSION_MINOR 1
#define PDQ_VERSION_PATCH 0

typedef enum {
    PdqOk = 0,
    PdqErrorInvalidParam,
    PdqErrorBufferTooSmall,
    PdqErrorTimeout,
    PdqErrorNoMemory,
    PdqErrorNotConnected,
    PdqErrorAuthFailed,
    PdqErrorInvalidJob,
    PdqErrorNvsRead,
    PdqErrorNvsWrite
} PdqError_t;

typedef struct {
    uint32_t State[8];
    uint8_t  Buffer[64];
    uint32_t ByteCount;
} PdqSha256Context_t;

typedef struct __attribute__((aligned(4))) {
    uint8_t  Midstate[32];
    uint8_t  BlockTail[64];
    uint32_t NonceStart;
    uint32_t NonceEnd;
    uint32_t Target[8];
    uint64_t JobId;
} PdqMiningJob_t;

typedef struct {
    uint32_t HashRate;
    uint64_t TotalHashes;
    uint32_t SharesAccepted;
    uint32_t SharesRejected;
    uint32_t BlocksFound;
    uint32_t Uptime;
    float    Temperature;
} PdqMinerStats_t;

#define PDQ_MAX_SSID_LEN       32
#define PDQ_MAX_PASSWORD_LEN   64
#define PDQ_MAX_HOST_LEN       64
#define PDQ_MAX_WALLET_LEN     64
#define PDQ_MAX_WORKER_LEN     32

typedef struct {
    char     Ssid[PDQ_MAX_SSID_LEN + 1];
    char     Password[PDQ_MAX_PASSWORD_LEN + 1];
} PdqWifiConfig_t;

typedef struct {
    char     Host[PDQ_MAX_HOST_LEN + 1];
    uint16_t Port;
} PdqPoolConfig_t;

typedef struct {
    PdqWifiConfig_t Wifi;
    PdqPoolConfig_t PrimaryPool;
    PdqPoolConfig_t BackupPool;
    char            WalletAddress[PDQ_MAX_WALLET_LEN + 1];
    char            WorkerName[PDQ_MAX_WORKER_LEN + 1];
    uint8_t         DisplayMode;
} PdqDeviceConfig_t;

#endif
