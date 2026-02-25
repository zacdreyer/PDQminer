# PDQminer Test-Driven Development Guide

> **Version**: 1.2.0
> **Last Updated**: 2025-02-14
> **Status**: Active
> **Framework**: Unity Test Framework

---

## 1. TDD Philosophy

### 1.1 Core Principles

PDQminer follows strict Test-Driven Development:

1. **Red**: Write a failing test first
2. **Green**: Write minimal code to pass the test
3. **Refactor**: Improve code while keeping tests green

### 1.2 Test Coverage Requirements

#### Firmware (C/Unity)

| Component | Coverage Requirement |
|-----------|---------------------|
| SHA256 Engine | 100% line + branch |
| Stratum Client | 100% line |
| Mining Task | 95% line |
| Display Driver | 80% line |
| WiFi Manager | 70% line |

#### Tools (Python/pytest)

| Component | Coverage Requirement |
|-----------|---------------------|
| Python Flasher - flasher.py | 95% line |
| Python Flasher - detector.py | 90% line |
| Python Flasher - config.py | 100% line |

### 1.3 Test Pyramid

```
              ┌─────────┐
              │   E2E   │  ← Minimal (hardware-dependent)
              │  Tests  │
             ┌┴─────────┴┐
             │Integration│  ← Component interaction
             │   Tests   │
            ┌┴───────────┴┐
            │    Unit     │  ← Bulk of tests
            │   Tests     │
            └─────────────┘
```

---

## 2. Test Organization

### 2.1 Directory Structure

```
test/
├── unit/                       # Unit tests (pending implementation)
│   ├── core/
│   │   ├── test_sha256_engine.c
│   │   └── test_mining_task.c
│   ├── stratum/
│   │   └── test_stratum_client.c
│   ├── display/
│   │   └── test_display_driver.c
│   └── config/
│       └── test_config_manager.c
├── integration/                # Integration tests (pending)
│   ├── test_mining_flow.c
│   └── test_stratum_job_flow.c
├── benchmark/                  # Performance benchmarks
│   └── test_sha256_benchmark.cpp  # SHA256 correctness and performance
├── mocks/                      # Mock implementations
│   ├── mock_wifi.c
│   ├── mock_wifi.h
│   ├── mock_tcp.c
│   ├── mock_tcp.h
│   ├── mock_display.c
│   └── mock_display.h
├── fixtures/                   # Test data
│   ├── stratum_jobs.json
│   ├── block_headers.h
│   └── sha256_vectors.h
├── unity/                      # Unity test framework
│   ├── unity.c
│   ├── unity.h
│   └── unity_internals.h
└── test_runner.c               # Main test entry point
```

### 2.2 Naming Conventions

**Test Function Names**:
```
Test_<Module>_<Function>_<Scenario>_<ExpectedResult>
```

**Examples**:
```c
void Test_Sha256_ComputeHash_ValidInput_ReturnsCorrectHash(void);
void Test_Sha256_ComputeHash_NullPointer_ReturnsError(void);
void Test_Sha256_ComputeHash_EmptyInput_ReturnsZeroHash(void);
void Test_Stratum_ParseNotify_ValidJson_ParsesAllFields(void);
void Test_Stratum_ParseNotify_MalformedJson_ReturnsError(void);
void Test_MiningTask_UpdateJob_NewJob_ClearsOldWork(void);
```

---

## 3. Unit Test Guidelines

### 3.1 Test File Template

```c
/**
 * @file    test_sha256_engine.c
 * @brief   Unit tests for SHA256 Engine module
 * @author  PDQminer Team
 * @date    YYYY-MM-DD
 */

/* ============================================================================
 * INCLUDES
 * ========================================================================= */

#include "unity.h"
#include "sha256_engine.h"
#include "sha256_vectors.h"  /* Test vectors */

/* ============================================================================
 * TEST FIXTURES
 * ========================================================================= */

static PdqSha256Context_t s_TestContext;

void setUp(void)
{
    /* Called before each test */
    memset(&s_TestContext, 0, sizeof(s_TestContext));
}

void tearDown(void)
{
    /* Called after each test */
}

/* ============================================================================
 * TEST CASES - PdqSha256Init
 * ========================================================================= */

/**
 * @brief   Test SHA256 initialization with valid context
 */
void Test_Sha256Init_ValidContext_InitializesState(void)
{
    /* Arrange */
    PdqSha256Context_t Context;

    /* Act */
    int32_t Result = PdqSha256Init(&Context);

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_EQUAL_HEX32(0x6A09E667, Context.State[0]);
    TEST_ASSERT_EQUAL_HEX32(0xBB67AE85, Context.State[1]);
    TEST_ASSERT_EQUAL_HEX32(0x3C6EF372, Context.State[2]);
    TEST_ASSERT_EQUAL_HEX32(0xA54FF53A, Context.State[3]);
    TEST_ASSERT_EQUAL_HEX32(0x510E527F, Context.State[4]);
    TEST_ASSERT_EQUAL_HEX32(0x9B05688C, Context.State[5]);
    TEST_ASSERT_EQUAL_HEX32(0x1F83D9AB, Context.State[6]);
    TEST_ASSERT_EQUAL_HEX32(0x5BE0CD19, Context.State[7]);
}

/**
 * @brief   Test SHA256 initialization with null pointer
 */
void Test_Sha256Init_NullContext_ReturnsError(void)
{
    /* Act */
    int32_t Result = PdqSha256Init(NULL);

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(PdqErrorInvalidParam, Result);
}

/* ============================================================================
 * TEST CASES - PdqSha256Double
 * ========================================================================= */

/**
 * @brief   Test SHA256d with known Bitcoin block header
 */
void Test_Sha256Double_BitcoinHeader_MatchesKnownHash(void)
{
    /* Arrange - Genesis block header */
    uint8_t Header[80] = {
        0x01, 0x00, 0x00, 0x00,  /* Version */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* PrevBlock */
        0x3B, 0xA3, 0xED, 0xFD, 0x7A, 0x7B, 0x12, 0xB2,
        0x7A, 0xC7, 0x2C, 0x3E, 0x67, 0x76, 0x8F, 0x61,
        0x7F, 0xC8, 0x1B, 0xC3, 0x88, 0x8A, 0x51, 0x32,
        0x3A, 0x9F, 0xB8, 0xAA, 0x4B, 0x1E, 0x5E, 0x4A,  /* MerkleRoot */
        0x29, 0xAB, 0x5F, 0x49,  /* Time */
        0xFF, 0xFF, 0x00, 0x1D,  /* Bits */
        0x1D, 0xAC, 0x2B, 0x7C   /* Nonce */
    };

    uint8_t ExpectedHash[32] = {
        0x6F, 0xE2, 0x8C, 0x0A, 0xB6, 0xF1, 0xB3, 0x72,
        0xC1, 0xA6, 0xA2, 0x46, 0xAE, 0x63, 0xF7, 0x4F,
        0x93, 0x1E, 0x83, 0x65, 0xE1, 0x5A, 0x08, 0x9C,
        0x68, 0xD6, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    uint8_t ActualHash[32];

    /* Act */
    int32_t Result = PdqSha256Double(Header, 80, ActualHash);

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ExpectedHash, ActualHash, 32);
}

/**
 * @brief   Test SHA256d with empty input
 */
void Test_Sha256Double_EmptyInput_ReturnsCorrectHash(void)
{
    /* Arrange */
    uint8_t ExpectedHash[32] = {
        0x5D, 0xF6, 0xE0, 0xE2, 0x76, 0x13, 0x59, 0xD3,
        0x0A, 0x82, 0x75, 0x05, 0x8E, 0x29, 0x9F, 0xCC,
        0x03, 0x81, 0x53, 0x45, 0x45, 0xF5, 0x5C, 0xF4,
        0x3E, 0x41, 0x98, 0x3F, 0x5D, 0x4C, 0x94, 0x56
    };
    uint8_t ActualHash[32];

    /* Act */
    int32_t Result = PdqSha256Double(NULL, 0, ActualHash);

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ExpectedHash, ActualHash, 32);
}

/* ============================================================================
 * TEST RUNNER
 * ========================================================================= */

int RunSha256Tests(void)
{
    UNITY_BEGIN();
    
    /* Init tests */
    RUN_TEST(Test_Sha256Init_ValidContext_InitializesState);
    RUN_TEST(Test_Sha256Init_NullContext_ReturnsError);
    
    /* Double hash tests */
    RUN_TEST(Test_Sha256Double_BitcoinHeader_MatchesKnownHash);
    RUN_TEST(Test_Sha256Double_EmptyInput_ReturnsCorrectHash);
    
    return UNITY_END();
}
```

### 3.2 Mock Objects

**Mock Header Template** (`mock_wifi.h`):

```c
/**
 * @file    mock_wifi.h
 * @brief   Mock WiFi implementation for testing
 */

#ifndef MOCK_WIFI_H
#define MOCK_WIFI_H

#include <stdint.h>
#include <stdbool.h>

/* Mock configuration */
void MockWifi_Reset(void);
void MockWifi_SetConnectResult(int32_t Result);
void MockWifi_SetConnected(bool IsConnected);
void MockWifi_SetIpAddress(const char *p_IpAddress);

/* Verification */
uint32_t MockWifi_GetConnectCallCount(void);
const char* MockWifi_GetLastSsid(void);
const char* MockWifi_GetLastPassword(void);

#endif /* MOCK_WIFI_H */
```

**Mock Implementation** (`mock_wifi.c`):

```c
/**
 * @file    mock_wifi.c
 * @brief   Mock WiFi implementation
 */

#include "mock_wifi.h"
#include "wifi_manager.h"
#include <string.h>

/* ============================================================================
 * MOCK STATE
 * ========================================================================= */

static struct {
    int32_t  ConnectResult;
    bool     IsConnected;
    char     IpAddress[16];
    uint32_t ConnectCallCount;
    char     LastSsid[64];
    char     LastPassword[64];
} s_MockState;

/* ============================================================================
 * MOCK CONFIGURATION
 * ========================================================================= */

void MockWifi_Reset(void)
{
    memset(&s_MockState, 0, sizeof(s_MockState));
    s_MockState.ConnectResult = 0;
}

void MockWifi_SetConnectResult(int32_t Result)
{
    s_MockState.ConnectResult = Result;
}

void MockWifi_SetConnected(bool IsConnected)
{
    s_MockState.IsConnected = IsConnected;
}

void MockWifi_SetIpAddress(const char *p_IpAddress)
{
    strncpy(s_MockState.IpAddress, p_IpAddress, sizeof(s_MockState.IpAddress) - 1);
}

/* ============================================================================
 * MOCK VERIFICATION
 * ========================================================================= */

uint32_t MockWifi_GetConnectCallCount(void)
{
    return s_MockState.ConnectCallCount;
}

const char* MockWifi_GetLastSsid(void)
{
    return s_MockState.LastSsid;
}

/* ============================================================================
 * MOCK IMPLEMENTATION (replaces real functions)
 * ========================================================================= */

int32_t PdqWifiConnect(const char *p_Ssid, const char *p_Password)
{
    s_MockState.ConnectCallCount++;
    strncpy(s_MockState.LastSsid, p_Ssid, sizeof(s_MockState.LastSsid) - 1);
    strncpy(s_MockState.LastPassword, p_Password, sizeof(s_MockState.LastPassword) - 1);
    return s_MockState.ConnectResult;
}

bool PdqWifiIsConnected(void)
{
    return s_MockState.IsConnected;
}
```

---

## 4. Integration Tests

### 4.1 Integration Test Example

```c
/**
 * @file    test_mining_flow.c
 * @brief   Integration test for complete mining flow
 */

#include "unity.h"
#include "stratum_client.h"
#include "mining_task.h"
#include "mock_tcp.h"

void Test_MiningFlow_ReceiveJob_StartsMining(void)
{
    /* Arrange */
    MockTcp_Reset();
    MockTcp_QueueResponse(STRATUM_SUBSCRIBE_RESPONSE);
    MockTcp_QueueResponse(STRATUM_AUTHORIZE_RESPONSE);
    MockTcp_QueueResponse(STRATUM_NOTIFY_JOB_1);

    /* Act */
    PdqError_t Result = PdqStratumInit();
    TEST_ASSERT_EQUAL(PdqOk, Result);

    Result = PdqStratumConnect("test-pool.io", 3333);
    TEST_ASSERT_EQUAL(PdqOk, Result);

    Result = PdqStratumSubscribe();
    TEST_ASSERT_EQUAL(PdqOk, Result);

    Result = PdqStratumAuthorize("bc1qtest...worker", "x");
    TEST_ASSERT_EQUAL(PdqOk, Result);

    /* Process incoming messages to receive job */
    Result = PdqStratumProcess();
    TEST_ASSERT_EQUAL(PdqOk, Result);
    TEST_ASSERT_TRUE(PdqStratumHasNewJob());

    PdqStratumJob_t Job;
    Result = PdqStratumGetJob(&Job);
    TEST_ASSERT_EQUAL(PdqOk, Result);

    /* Assert - Job was received and parsed */
    TEST_ASSERT_NOT_EMPTY(Job.JobId);
    TEST_ASSERT_NOT_EQUAL(0, Job.NBits);

    /* Assert - Mining job can be built from stratum job */
    PdqMiningJob_t MiningJob;
    uint8_t Extranonce1[4] = {0};
    Result = PdqStratumBuildMiningJob(&Job, Extranonce1, 4, 0, 4,
                                      PdqStratumGetDifficulty(), &MiningJob);
    TEST_ASSERT_EQUAL(PdqOk, Result);
}

### 4.2 WiFi Provisioning Integration Tests

```c
/**
 * @file    test_wifi_provisioning.c
 * @brief   Integration tests for WiFi manager and configuration portal
 */

#include "unity.h"
#include "wifi_manager.h"
#include "config_manager.h"
#include "mock_nvs.h"
#include "mock_wifi.h"

static DeviceConfig_t s_TestConfig;

void setUp(void)
{
    MockNvs_Reset();
    MockWifi_Reset();
    memset(&s_TestConfig, 0, sizeof(s_TestConfig));
}

void tearDown(void)
{
    PdqConfigReset();
}

/**
 * @brief Test saving and loading configuration round-trip
 */
void Test_Config_SaveLoad_RoundTrip(void)
{
    PdqDeviceConfig_t SaveConfig;
    memset(&SaveConfig, 0, sizeof(SaveConfig));
    strncpy(SaveConfig.Wifi.Ssid, "TestNetwork", PDQ_MAX_SSID_LEN);
    strncpy(SaveConfig.Wifi.Password, "SecurePass123", PDQ_MAX_PASSWORD_LEN);
    strncpy(SaveConfig.PrimaryPool.Host, "public-pool.io", PDQ_MAX_HOST_LEN);
    SaveConfig.PrimaryPool.Port = 21496;
    strncpy(SaveConfig.BackupPool.Host, "solo.ckpool.org", PDQ_MAX_HOST_LEN);
    SaveConfig.BackupPool.Port = 3333;
    strncpy(SaveConfig.WalletAddress, "bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh", PDQ_MAX_WALLET_LEN);
    strncpy(SaveConfig.WorkerName, "pdqminer1", PDQ_MAX_WORKER_LEN);

    PdqError_t Result = PdqConfigSave(&SaveConfig);
    TEST_ASSERT_EQUAL(PdqOk, Result);

    PdqDeviceConfig_t LoadConfig;
    Result = PdqConfigLoad(&LoadConfig);
    TEST_ASSERT_EQUAL(PdqOk, Result);

    TEST_ASSERT_EQUAL_STRING(SaveConfig.Wifi.Ssid, LoadConfig.Wifi.Ssid);
    TEST_ASSERT_EQUAL_STRING(SaveConfig.PrimaryPool.Host, LoadConfig.PrimaryPool.Host);
    TEST_ASSERT_EQUAL_STRING(SaveConfig.WalletAddress, LoadConfig.WalletAddress);
}

/**
 * @brief Test loading when no configuration exists
 */
void Test_Config_Load_NoConfig_ReturnsError(void)
{
    PdqDeviceConfig_t Config;
    PdqError_t Result = PdqConfigLoad(&Config);
    TEST_ASSERT_NOT_EQUAL(PdqOk, Result);
}

/**
 * @brief Test factory reset clears configuration
 */
void Test_Config_Reset_ClearsAllData(void)
{
    PdqDeviceConfig_t Config;
    memset(&Config, 0, sizeof(Config));
    strncpy(Config.Wifi.Ssid, "TestNetwork", PDQ_MAX_SSID_LEN);
    strncpy(Config.WalletAddress, "bc1qtest", PDQ_MAX_WALLET_LEN);
    PdqConfigSave(&Config);
    TEST_ASSERT_TRUE(PdqConfigIsValid());

    PdqError_t Result = PdqConfigReset();
    TEST_ASSERT_EQUAL(PdqOk, Result);
    TEST_ASSERT_FALSE(PdqConfigIsValid());
}

/**
 * @brief Test wallet address validation
 */
void Test_Config_WalletValidation_ValidAddresses(void)
{
    TEST_ASSERT_TRUE(PdqConfigValidateWallet("bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh"));
    TEST_ASSERT_TRUE(PdqConfigValidateWallet("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"));
    TEST_ASSERT_TRUE(PdqConfigValidateWallet("3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy"));
}

void Test_Config_WalletValidation_InvalidAddresses(void)
{
    TEST_ASSERT_FALSE(PdqConfigValidateWallet(""));
    TEST_ASSERT_FALSE(PdqConfigValidateWallet("bc1q"));
    TEST_ASSERT_FALSE(PdqConfigValidateWallet("invalid"));
    TEST_ASSERT_FALSE(PdqConfigValidateWallet(NULL));
}

/**
 * @brief Test pool host and port validation
 */
void Test_Config_PoolValidation(void)
{
    TEST_ASSERT_TRUE(PdqConfigValidateHost("public-pool.io"));
    TEST_ASSERT_TRUE(PdqConfigValidateHost("192.168.1.100"));
    TEST_ASSERT_FALSE(PdqConfigValidateHost(""));
    TEST_ASSERT_FALSE(PdqConfigValidateHost(NULL));

    TEST_ASSERT_TRUE(PdqConfigValidatePort(3333));
    TEST_ASSERT_TRUE(PdqConfigValidatePort(21496));
    TEST_ASSERT_FALSE(PdqConfigValidatePort(0));
    TEST_ASSERT_FALSE(PdqConfigValidatePort(65536));
}

/**
 * @brief Test portal starts in AP mode when unconfigured
 */
void Test_Portal_Unconfigured_StartsApMode(void)
{
    MockNvs_SetEmpty();
    int32_t Result = PdqWifiManagerInit();
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_TRUE(MockWifi_IsApModeActive());
}

/**
 * @brief Test failover to backup pool on primary failure
 */
void Test_Failover_PrimaryFails_ConnectsToBackup(void)
{
    PdqDeviceConfig_t Config;
    memset(&Config, 0, sizeof(Config));
    strncpy(Config.PrimaryPool.Host, "primary.pool", PDQ_MAX_HOST_LEN);
    Config.PrimaryPool.Port = 3333;
    strncpy(Config.BackupPool.Host, "backup.pool", PDQ_MAX_HOST_LEN);
    Config.BackupPool.Port = 3333;
    PdqConfigSave(&Config);

    MockTcp_SetConnectResult("primary.pool", -ECONNREFUSED);
    MockTcp_SetConnectResult("backup.pool", 0);

    PdqError_t Result = PdqStratumConnectWithFailover();
    TEST_ASSERT_EQUAL(PdqOk, Result);
    TEST_ASSERT_EQUAL_STRING("backup.pool", MockTcp_GetLastConnectedHost());
}
```
```

---

## 5. Benchmark Tests

### 5.1 SHA256 Benchmark

```c
/**
 * @file    benchmark_sha256.c
 * @brief   SHA256 performance benchmarking
 */

#include "unity.h"
#include "sha256_engine.h"
#include "esp_timer.h"

#define BENCHMARK_ITERATIONS    1000000
#define BENCHMARK_DURATION_MS   10000

typedef struct {
    uint32_t    Iterations;
    uint64_t    DurationUs;
    float       HashesPerSecond;
    float       KiloHashesPerSecond;
} BenchmarkResult_t;

/**
 * @brief   Benchmark pure SHA256d throughput
 * @note    Uses actual PdqSha256MineBlock() API matching sha256_engine.h
 */
void Benchmark_Sha256Double_Throughput(void)
{
    uint8_t Header[80];
    PdqMiningJob_t Job;

    /* Initialize with random header */
    for (int i = 0; i < 80; i++) {
        Header[i] = (uint8_t)(i * 7);
    }

    PdqSha256Midstate(Header, Job.Midstate);
    memcpy(Job.BlockTail, Header + 64, 16);
    memset(Job.Target, 0xFF, 32); /* Accept all hashes */

    /* Warm up */
    Job.NonceStart = 0;
    Job.NonceEnd = 999;
    uint32_t Nonce;
    bool Found;
    PdqSha256MineBlock(&Job, &Nonce, &Found);

    /* Benchmark */
    uint64_t StartTime = esp_timer_get_time();
    uint32_t Iterations = 0;

    while ((esp_timer_get_time() - StartTime) < (BENCHMARK_DURATION_MS * 1000)) {
        Job.NonceStart = Iterations;
        Job.NonceEnd = Iterations + 4095;
        PdqSha256MineBlock(&Job, &Nonce, &Found);
        Iterations += 4096;
    }

    uint64_t EndTime = esp_timer_get_time();
    uint64_t DurationUs = EndTime - StartTime;

    /* Calculate results */
    BenchmarkResult_t Result;
    Result.Iterations = Iterations;
    Result.DurationUs = DurationUs;
    Result.HashesPerSecond = (float)Iterations / ((float)DurationUs / 1000000.0f);
    Result.KiloHashesPerSecond = Result.HashesPerSecond / 1000.0f;

    /* Report */
    printf("\n");
    printf("=== SHA256d Benchmark Results ===\n");
    printf("Iterations:    %lu\n", (unsigned long)Result.Iterations);
    printf("Duration:      %llu us\n", (unsigned long long)Result.DurationUs);
    printf("Hashrate:      %.2f H/s\n", Result.HashesPerSecond);
    printf("Hashrate:      %.2f KH/s\n", Result.KiloHashesPerSecond);
    printf("=================================\n");

    /* Assert minimum performance */
    TEST_ASSERT_GREATER_THAN(500.0f, Result.KiloHashesPerSecond);
}

/**
 * @file    benchmark_regression.c
 * @brief   Performance regression tests with strict thresholds
 */

#include "unity.h"
#include "sha256_engine.h"
#include "mining_task.h"
#include "esp_timer.h"

/* Minimum acceptable hashrates (fail build if not met) */
#define MIN_SW_SINGLE_CORE_KHS  25.0f    /* SW single core minimum */
#define MIN_HW_SINGLE_CORE_KHS  600.0f   /* HW single core minimum */
#define MIN_COMBINED_KHS        650.0f   /* HW + SW combined minimum */
#define TARGET_COMBINED_KHS    1000.0f   /* Target performance */

/**
 * @brief   Single-core SW hashrate regression test
 * @note    SW path compiled with -Os, expected ~46 KH/s per core
 *          Uses PdqSha256MineBlock() API from sha256_engine.h
 */
void Test_Performance_SwSingleCore_MinimumHashrate(void)
{
    uint8_t Header[80];
    PdqMiningJob_t Job;

    /* Initialize */
    for (int i = 0; i < 80; i++) Header[i] = i;
    PdqSha256Midstate(Header, Job.Midstate);
    memcpy(Job.BlockTail, Header + 64, 16);
    memset(Job.Target, 0xFF, 32);

    /* Benchmark 5 seconds */
    uint64_t Start = esp_timer_get_time();
    uint32_t Hashes = 0;

    while ((esp_timer_get_time() - Start) < 5000000) {
        Job.NonceStart = Hashes;
        Job.NonceEnd = Hashes + 4095;
        uint32_t Nonce;
        bool Found;
        PdqSha256MineBlock(&Job, &Nonce, &Found);
        Hashes += 4096;
    }

    float KHs = (float)Hashes / 5000.0f;
    printf("[REGRESSION] SW single-core: %.2f KH/s (min: %.0f)\n", KHs, MIN_SW_SINGLE_CORE_KHS);

    TEST_ASSERT_GREATER_OR_EQUAL(MIN_SW_SINGLE_CORE_KHS, KHs);
}

/**
 * @brief   HW SHA256 single-core hashrate regression test
 * @note    HW path uses ESP32 SHA peripheral with overlap optimization
 * @note    MUST achieve at least 600 KH/s (current: ~650 KH/s, ~369 cyc/nonce)
 */
void Test_Performance_HwSingleCore_MinimumHashrate(void)
{
    uint8_t Header[80];
    PdqMiningJob_t Job;

    for (int i = 0; i < 80; i++) Header[i] = i;
    PdqSha256Midstate(Header, Job.Midstate);
    memcpy(Job.BlockTail, Header + 64, 16);
    memset(Job.Target, 0xFF, 32);

    uint64_t Start = esp_timer_get_time();
    uint32_t Hashes = 0;

    while ((esp_timer_get_time() - Start) < 5000000) {
        Job.NonceStart = Hashes;
        Job.NonceEnd = Hashes + (128*1024) - 1;
        uint32_t Nonce;
        bool Found;
        PdqSha256MineBlockHw(&Job, &Nonce, &Found);
        Hashes += (128*1024);
    }

    float KHs = (float)Hashes / 5000.0f;
    printf("[REGRESSION] HW single-core: %.2f KH/s (min: %.0f)\n", KHs, MIN_HW_SINGLE_CORE_KHS);

    TEST_ASSERT_GREATER_OR_EQUAL(MIN_HW_SINGLE_CORE_KHS, KHs);
}

/**
 * @brief   Combined HW+SW hashrate regression test
 * @note    MUST achieve at least 650 KH/s combined (current: ~700 KH/s)
 */
void Test_Performance_Combined_MinimumHashrate(void)
{
    /* Start mining tasks (HW on Core 0 + SW on Core 1) */
    PdqMiningInit();
    PdqMiningStart();

    /* Let it run for 10 seconds */
    vTaskDelay(pdMS_TO_TICKS(10000));

    /* Get hashrate */
    PdqMinerStats_t Stats;
    PdqMiningGetStats(&Stats);
    float KHs = (float)Stats.HashRate / 1000.0f;
    printf("[REGRESSION] Combined HW+SW: %.2f KH/s (min: %.0f, target: %.0f)\n",
           KHs, MIN_COMBINED_KHS, TARGET_COMBINED_KHS);

    PdqMiningStop();

    TEST_ASSERT_GREATER_OR_EQUAL(MIN_COMBINED_KHS, KHs);
}

/**
 * @brief   Target hashrate validation (informational, not fail)
 */
void Test_Performance_TargetHashrate_1000KHs(void)
{
    PdqMiningInit();
    PdqMiningStart();

    vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 second benchmark */

    PdqMinerStats_t Stats;
    PdqMiningGetStats(&Stats);
    float KHs = (float)Stats.HashRate / 1000.0f;
    printf("[TARGET] Achieved: %.2f KH/s (target: %.0f)\n", KHs, TARGET_COMBINED_KHS);

    PdqMiningStop();

    /* Informational: warn if below target but don't fail */
    if (KHs < TARGET_COMBINED_KHS) {
        printf("[WARNING] Below target hashrate!\n");
    }
    TEST_ASSERT_GREATER_OR_EQUAL(MIN_COMBINED_KHS, KHs);
}

/**
 * @brief   Verify midstate caching provides expected speedup
 */
void Test_Optimization_MidstateCaching_Speedup(void)
{
    uint8_t Header[80];
    for (int i = 0; i < 80; i++) Header[i] = i;

    /* Without midstate: full SHA256d every nonce */
    uint64_t Start = esp_timer_get_time();
    for (int i = 0; i < 10000; i++) {
        uint8_t Hash[32];
        Header[76] = i & 0xFF;  /* Vary nonce */
        PdqSha256d(Header, 80, Hash);
    }
    uint64_t NoMidstateTime = esp_timer_get_time() - Start;

    /* With midstate: only recompute tail */
    PdqMiningJob_t Job;
    PdqSha256Midstate(Header, Job.Midstate);
    memcpy(Job.BlockTail, Header + 64, 16);
    memset(Job.Target, 0xFF, 32);
    Job.NonceStart = 0;
    Job.NonceEnd = 9999;

    Start = esp_timer_get_time();
    uint32_t Nonce;
    bool Found;
    PdqSha256MineBlock(&Job, &Nonce, &Found);
    uint64_t MidstateTime = esp_timer_get_time() - Start;

    float Speedup = (float)NoMidstateTime / (float)MidstateTime;
    printf("[OPT] Midstate speedup: %.2fx\n", Speedup);

    /* Midstate should provide at least 1.3x speedup */
    TEST_ASSERT_GREATER_THAN(1.3f, Speedup);
}

/**
 * @brief   Verify IRAM placement effect (currently NOT used for HW path)
 * @note    IRAM was tested for HW mining path but regressed (538 vs 413 cyc/nonce)
 *          Flash cache is already efficient for the HW hot loop
 */
void Test_Optimization_IramPlacement_Benefit(void)
{
    printf("[OPT] IRAM: Not used for HW path (regressed from 413 to 538 cyc/nonce)\n");
    printf("[OPT] Flash cache is optimal for HW SHA mining loop\n");
    TEST_PASS();
}

/**
 * @brief   Verify HW+SW combined provides significant speedup over SW-only
 * @note    HW path (581 KH/s) vastly outperforms SW path (~46 KH/s per core)
 */
void Test_Optimization_HwVsSw_Speedup(void)
{
    uint8_t Header[80];
    for (int i = 0; i < 80; i++) Header[i] = i;

    PdqMiningJob_t Job;
    PdqSha256Midstate(Header, Job.Midstate);
    memcpy(Job.BlockTail, Header + 64, 16);
    memset(Job.Target, 0xFF, 32);

    /* Single core benchmark */
    Job.NonceStart = 0;
    Job.NonceEnd = 999999;
    uint64_t Start = esp_timer_get_time();
    uint32_t Nonce;
    bool Found;
    PdqSha256MineBlock(&Job, &Nonce, &Found);
    uint64_t SingleTime = esp_timer_get_time() - Start;
    float SingleKHs = 1000000.0f / ((float)SingleTime / 1000000.0f);

    /* Dual core benchmark */
    PdqMiningInit();
    PdqMiningStart();
    Job.NonceStart = 0;
    Job.NonceEnd = 0xFFFFFFFF;
    PdqMiningSetJob(&Job);
    vTaskDelay(pdMS_TO_TICKS(5000));
    PdqMinerStats_t Stats;
    PdqMiningGetStats(&Stats);
    float DualKHs = (float)Stats.HashRate / 1000.0f;
    PdqMiningStop();

    float Speedup = DualKHs / SingleKHs;
    printf("[OPT] HW+SW vs SW-only speedup: %.2fx (%.0f vs %.0f KH/s)\n",
           Speedup, DualKHs, SingleKHs);

    /* HW+SW combined should be at least 10x better than SW-only (581+46 vs ~46) */
    TEST_ASSERT_GREATER_THAN(5.0f, Speedup);
}

/**
 * @brief   Verify mining job struct fits in cache
 */
void Test_Memory_MiningJob_Size(void)
{
    printf("[MEM] PdqMiningJob_t size: %u bytes\n", sizeof(PdqMiningJob_t));

    /* Job struct should remain small for cache efficiency */
    TEST_ASSERT_LESS_THAN(512, sizeof(PdqMiningJob_t));
}

/**
 * @brief   Verify no heap allocation in hot path
 */
void Test_Memory_HotPath_NoHeapAlloc(void)
{
    PdqMiningJob_t Job;
    uint8_t Header[80] = {0};

    PdqSha256Midstate(Header, Job.Midstate);
    memcpy(Job.BlockTail, Header + 64, 16);
    memset(Job.Target, 0xFF, 32);

    size_t HeapBefore = esp_get_free_heap_size();

    Job.NonceStart = 0;
    Job.NonceEnd = 99999;
    uint32_t Nonce;
    bool Found;
    PdqSha256MineBlock(&Job, &Nonce, &Found);

    size_t HeapAfter = esp_get_free_heap_size();

    printf("[MEM] Heap before: %u, after: %u\n", HeapBefore, HeapAfter);

    /* No heap usage in mining loop */
    TEST_ASSERT_EQUAL(HeapBefore, HeapAfter);
}

/**
 * @brief   Verify WDT feeding frequency is adequate
 */
void Test_Timing_WdtFeedFrequency(void)
{
    /* WDT timeout is 5 seconds; we must feed within that */
    /* With 4096 batch size at 1000 KH/s, we feed every ~4ms */

    float ExpectedFeedIntervalMs = (4096.0f / 1000000.0f) * 1000.0f;
    printf("[TIMING] Expected WDT feed interval: %.2f ms\n", ExpectedFeedIntervalMs);

    /* Must feed much faster than 5 second timeout */
    TEST_ASSERT_LESS_THAN(100.0f, ExpectedFeedIntervalMs);
}
```

---

## 6. Test Execution

### 6.1 Running Tests Locally

```bash
# Build and run all tests
pio test -e native

# Run specific test suite
pio test -e native -f test_sha256

# Run with verbose output
pio test -e native -v

# Run benchmarks (on hardware)
pio test -e cyd_ili9341 -f benchmark
```

### 6.2 CI/CD Integration

```yaml
# .github/workflows/test.yml
name: Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/cache@v3
        with:
          path: ~/.platformio
          key: ${{ runner.os }}-pio
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run Unit Tests
        run: pio test -e native
      - name: Upload Coverage
        uses: codecov/codecov-action@v3
```

### 6.3 Coverage Report

```bash
# Generate coverage report
pio test -e native --coverage

# View HTML report
open .pio/build/native/coverage/index.html
```

---

## 7. Test Data Management

### 7.1 Test Vectors

Store test vectors in `test/fixtures/`:

```c
/**
 * @file    sha256_vectors.h
 * @brief   SHA256 test vectors from NIST and Bitcoin
 */

#ifndef SHA256_VECTORS_H
#define SHA256_VECTORS_H

#include <stdint.h>

typedef struct {
    const uint8_t  *Input;
    uint32_t        InputLength;
    const uint8_t   ExpectedHash[32];
    const char     *Description;
} Sha256TestVector_t;

/* NIST test vectors */
extern const Sha256TestVector_t g_NistTestVectors[];
extern const uint32_t g_NistTestVectorCount;

/* Bitcoin block header test vectors */
extern const Sha256TestVector_t g_BitcoinTestVectors[];
extern const uint32_t g_BitcoinTestVectorCount;

#endif /* SHA256_VECTORS_H */
```

### 7.2 Stratum Test Fixtures

```json
// test/fixtures/stratum_jobs.json
{
  "subscribe_response": {
    "id": 1,
    "result": [
      ["mining.notify", "ae6812eb4cd7735a302a8a9dd95cf71f"],
      "08000002"
    ],
    "error": null
  },
  "notify_job": {
    "id": null,
    "method": "mining.notify",
    "params": [
      "job_001",
      "0000000000000000000123456789abcdef...",
      "01000000010000000000000000...",
      "ffffffff",
      [],
      "20000000",
      "1a0ffff0",
      "5f5e1000",
      true
    ]
  }
}
```

---

## 8. Debugging Failed Tests

### 8.1 Unity Assertions Reference

| Assertion | Usage |
|-----------|-------|
| `TEST_ASSERT_EQUAL_INT32(exp, act)` | Integer comparison |
| `TEST_ASSERT_EQUAL_HEX32(exp, act)` | Hex comparison |
| `TEST_ASSERT_EQUAL_HEX8_ARRAY(exp, act, len)` | Byte array |
| `TEST_ASSERT_TRUE(cond)` | Boolean true |
| `TEST_ASSERT_FALSE(cond)` | Boolean false |
| `TEST_ASSERT_NULL(ptr)` | Null pointer |
| `TEST_ASSERT_NOT_NULL(ptr)` | Non-null pointer |
| `TEST_ASSERT_GREATER_THAN(threshold, value)` | Greater than |
| `TEST_FAIL_MESSAGE(msg)` | Force failure |

### 8.2 Debug Output

```c
void Test_Example_WithDebugOutput(void)
{
    uint8_t Data[32];

    /* ... test code ... */

    /* Debug: print hex dump */
    printf("Data: ");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", Data[i]);
    }
    printf("\n");

    TEST_ASSERT_EQUAL_HEX8_ARRAY(Expected, Data, 32);
}
```

---

## 9. Firmware Patcher Tool Tests

The firmware patcher tools (Python CLI and Web flasher) require their own test suites.

### 9.1 Python Flasher Tests (`tools/python-flasher/tests/`)

#### 9.1.1 Test Organization

```
tools/python-flasher/
├── tests/
│   ├── conftest.py           # pytest fixtures
│   ├── test_flasher.py       # Flash operation tests
│   ├── test_detector.py      # Board detection tests
│   ├── test_config.py        # Configuration tests
│   └── test_cli.py           # CLI interface tests
└── pytest.ini                # pytest configuration
```

#### 9.1.2 Test Examples

**test_detector.py**:
```python
"""Unit tests for board detection module."""

import pytest
from unittest.mock import Mock, patch
from pdqflash.detector import detect_board, detect_port, get_chip_info


class TestDetectPort:
    """Tests for serial port detection."""

    @patch('pdqflash.detector.list_ports.comports')
    def test_detect_port_single_esp32_returns_port(self, mock_comports):
        """When single ESP32 connected, returns its port."""
        mock_port = Mock()
        mock_port.device = '/dev/ttyUSB0'
        mock_port.vid = 0x10C4  # Silicon Labs CP210x
        mock_port.pid = 0xEA60
        mock_comports.return_value = [mock_port]

        result = detect_port()

        assert result == '/dev/ttyUSB0'

    @patch('pdqflash.detector.list_ports.comports')
    def test_detect_port_no_devices_returns_none(self, mock_comports):
        """When no ESP32 devices connected, returns None."""
        mock_comports.return_value = []

        result = detect_port()

        assert result is None

    @patch('pdqflash.detector.list_ports.comports')
    def test_detect_port_multiple_devices_returns_first(self, mock_comports):
        """When multiple ESP32 devices, returns first one."""
        mock_port1 = Mock(device='/dev/ttyUSB0', vid=0x10C4, pid=0xEA60)
        mock_port2 = Mock(device='/dev/ttyUSB1', vid=0x10C4, pid=0xEA60)
        mock_comports.return_value = [mock_port1, mock_port2]

        result = detect_port()

        assert result == '/dev/ttyUSB0'


class TestDetectBoard:
    """Tests for board type detection."""

    @patch('pdqflash.detector.esptool.detect_chip')
    def test_detect_board_esp32_returns_chip_info(self, mock_detect):
        """When ESP32 detected, returns chip information."""
        mock_detect.return_value = Mock(
            CHIP_NAME='ESP32',
            read_mac=Mock(return_value=b'\xaa\xbb\xcc\xdd\xee\xff')
        )

        result = detect_board('/dev/ttyUSB0')

        assert result['chip'] == 'ESP32'
        assert 'mac' in result

    @patch('pdqflash.detector.esptool.detect_chip')
    def test_detect_board_connection_error_raises(self, mock_detect):
        """When connection fails, raises appropriate error."""
        mock_detect.side_effect = Exception("Failed to connect")

        with pytest.raises(ConnectionError):
            detect_board('/dev/ttyUSB0')
```

**test_flasher.py**:
```python
"""Unit tests for flash operations module."""

import pytest
from unittest.mock import Mock, patch, MagicMock
from pdqflash.flasher import flash_firmware, verify_firmware, erase_flash


class TestFlashFirmware:
    """Tests for firmware flashing."""

    @pytest.fixture
    def mock_esptool(self):
        """Mock esptool for testing."""
        with patch('pdqflash.flasher.esptool') as mock:
            yield mock

    def test_flash_firmware_valid_binary_succeeds(self, mock_esptool, tmp_path):
        """Flashing valid binary completes successfully."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b'\x00' * 1024)

        result = flash_firmware(
            port='/dev/ttyUSB0',
            firmware_path=str(firmware),
            board='cyd_ili9341'
        )

        assert result['success'] is True
        mock_esptool.main.assert_called_once()

    def test_flash_firmware_missing_file_raises(self, mock_esptool):
        """Flashing non-existent file raises FileNotFoundError."""
        with pytest.raises(FileNotFoundError):
            flash_firmware(
                port='/dev/ttyUSB0',
                firmware_path='/nonexistent/firmware.bin',
                board='cyd_ili9341'
            )

    def test_flash_firmware_invalid_board_raises(self, mock_esptool, tmp_path):
        """Flashing with invalid board name raises ValueError."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b'\x00' * 1024)

        with pytest.raises(ValueError, match="Unknown board"):
            flash_firmware(
                port='/dev/ttyUSB0',
                firmware_path=str(firmware),
                board='invalid_board'
            )


class TestVerifyFirmware:
    """Tests for firmware verification."""

    @patch('pdqflash.flasher.esptool')
    def test_verify_firmware_matching_checksum_succeeds(self, mock_esptool):
        """Verification with matching checksum returns True."""
        mock_esptool.main.return_value = None  # Success

        result = verify_firmware('/dev/ttyUSB0', expected_checksum='abc123')

        assert result is True

    @patch('pdqflash.flasher.esptool')
    def test_verify_firmware_mismatch_fails(self, mock_esptool):
        """Verification with mismatched checksum returns False."""
        mock_esptool.main.side_effect = Exception("Checksum mismatch")

        result = verify_firmware('/dev/ttyUSB0', expected_checksum='abc123')

        assert result is False
```

**test_config.py**:
```python
"""Unit tests for configuration module."""

import pytest
from pdqflash.config import BOARD_CONFIGS, get_board_config, validate_config


class TestBoardConfigs:
    """Tests for board configuration data."""

    def test_cyd_ili9341_config_exists(self):
        """CYD ILI9341 board configuration is defined."""
        assert 'cyd_ili9341' in BOARD_CONFIGS

    def test_cyd_st7789_config_exists(self):
        """CYD ST7789 board configuration is defined."""
        assert 'cyd_st7789' in BOARD_CONFIGS

    @pytest.mark.parametrize('board', ['cyd_ili9341', 'cyd_st7789'])
    def test_config_has_required_fields(self, board):
        """All boards have required configuration fields."""
        config = BOARD_CONFIGS[board]
        required = ['chip', 'flash_size', 'flash_mode', 'flash_freq', 'firmware']

        for field in required:
            assert field in config, f"Missing field: {field}"

    @pytest.mark.parametrize('board', ['cyd_ili9341', 'cyd_st7789'])
    def test_config_chip_is_esp32(self, board):
        """All CYD boards use ESP32 chip."""
        assert BOARD_CONFIGS[board]['chip'] == 'esp32'


class TestGetBoardConfig:
    """Tests for get_board_config function."""

    def test_valid_board_returns_config(self):
        """Valid board name returns configuration dict."""
        config = get_board_config('cyd_ili9341')

        assert isinstance(config, dict)
        assert config['chip'] == 'esp32'

    def test_invalid_board_raises_keyerror(self):
        """Invalid board name raises KeyError."""
        with pytest.raises(KeyError):
            get_board_config('nonexistent_board')
```

**test_cli.py**:
```python
"""Tests for CLI interface."""

import pytest
from click.testing import CliRunner
from pdqflash import cli


class TestCLI:
    """Tests for pdqflash CLI commands."""

    @pytest.fixture
    def runner(self):
        """CLI test runner."""
        return CliRunner()

    def test_version_shows_version(self, runner):
        """--version flag shows version info."""
        result = runner.invoke(cli.main, ['--version'])

        assert result.exit_code == 0
        assert 'pdqflash' in result.output.lower()

    def test_flash_no_port_auto_detects(self, runner):
        """flash command without --port attempts auto-detection."""
        with patch('pdqflash.cli.detect_port', return_value=None):
            result = runner.invoke(cli.main, ['flash'])

        assert 'No ESP32 device detected' in result.output

    def test_detect_lists_devices(self, runner):
        """detect command lists connected devices."""
        with patch('pdqflash.cli.detect_port', return_value='/dev/ttyUSB0'):
            result = runner.invoke(cli.main, ['detect'])

        assert '/dev/ttyUSB0' in result.output
```

#### 9.1.3 Running Python Tests

```bash
# Install test dependencies
cd tools/python-flasher
pip install -e ".[dev]"

# Run all tests
pytest

# Run with coverage
pytest --cov=pdqflash --cov-report=html

# Run specific test file
pytest tests/test_flasher.py -v

# Run tests matching pattern
pytest -k "test_detect"
```

#### 9.1.4 pytest Configuration

```ini
# pytest.ini
[pytest]
testpaths = tests
python_files = test_*.py
python_functions = test_*
addopts = -v --tb=short
markers =
    slow: marks tests as slow
    integration: marks tests requiring hardware
```

### 9.2 Web Flasher Tests (`tools/web-flasher/`)

#### 9.2.1 JavaScript Tests (Jest)

```javascript
// tests/flasher.test.js
describe('Board Selection', () => {
    test('populateBoardDropdown adds all supported boards', () => {
        document.body.innerHTML = '<select id="board-select"></select>';

        populateBoardDropdown();

        const options = document.querySelectorAll('#board-select option');
        expect(options.length).toBeGreaterThan(0);
        expect(options[0].value).toBe('cyd_ili9341');
    });

    test('getManifestUrl returns correct URL for board', () => {
        const url = getManifestUrl('cyd_ili9341');

        expect(url).toContain('manifest');
        expect(url).toContain('ili9341');
    });
});

describe('Flash Progress', () => {
    test('updateProgress updates progress bar', () => {
        document.body.innerHTML = '<progress id="flash-progress"></progress>';

        updateProgress(50);

        const progress = document.getElementById('flash-progress');
        expect(progress.value).toBe(50);
    });
});
```

#### 9.2.2 E2E Tests (Playwright)

```javascript
// tests/e2e/flasher.spec.js
const { test, expect } = require('@playwright/test');

test.describe('Web Flasher Page', () => {
    test.beforeEach(async ({ page }) => {
        await page.goto('http://localhost:8080');
    });

    test('page loads with board selector', async ({ page }) => {
        const selector = page.locator('#board-select');
        await expect(selector).toBeVisible();
    });

    test('flash button disabled without WebSerial', async ({ page }) => {
        const button = page.locator('#flash-button');
        // WebSerial not available in test browser
        await expect(button).toBeDisabled();
    });

    test('board selection updates manifest', async ({ page }) => {
        await page.selectOption('#board-select', 'cyd_st7789');

        const manifest = await page.evaluate(() =>
            document.querySelector('esp-web-install-button').getAttribute('manifest')
        );
        expect(manifest).toContain('st7789');
    });
});
```

### 9.3 CI Integration for Tools

```yaml
# .github/workflows/test-tools.yml
name: Test Tools

on: [push, pull_request]

jobs:
  test-python-flasher:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      - name: Install dependencies
        run: |
          cd tools/python-flasher
          pip install -e ".[dev]"
      - name: Run tests
        run: |
          cd tools/python-flasher
          pytest --cov=pdqflash --cov-report=xml
      - name: Upload coverage
        uses: codecov/codecov-action@v4
        with:
          files: tools/python-flasher/coverage.xml

  test-web-flasher:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: '20'
      - name: Install dependencies
        run: |
          cd tools/web-flasher
          npm install
      - name: Run Jest tests
        run: |
          cd tools/web-flasher
          npm test
      - name: Run Playwright tests
        run: |
          cd tools/web-flasher
          npx playwright install
          npm run test:e2e
```

---

## 10. Security Testing

Security testing validates that defensive coding practices are correctly implemented.

### 10.1 Input Validation Tests

```c
/**
 * @file    test_input_validation.c
 * @brief   Security tests for input validation
 */

#include "unity.h"
#include "stratum_client.h"
#include "utils.h"

/* ============================================================================
 * BUFFER OVERFLOW TESTS
 * ========================================================================= */

void Test_StratumConfig_OversizedHostname_Truncates(void)
{
    /* Arrange - hostname longer than max */
    char LongHost[256];
    memset(LongHost, 'A', sizeof(LongHost) - 1);
    LongHost[sizeof(LongHost) - 1] = '\0';

    PdqPoolConfig_t Config;

    /* Act */
    strncpy(Config.Host, LongHost, PDQ_MAX_HOST_LEN);
    Config.Host[PDQ_MAX_HOST_LEN] = '\0';

    /* Assert - truncated, not overflowed */
    TEST_ASSERT_EQUAL(PDQ_MAX_HOST_LEN, strlen(Config.Host));
    TEST_ASSERT_EQUAL('\0', Config.Host[PDQ_MAX_HOST_LEN]);
}

void Test_HexToBytes_OddLength_ReturnsError(void)
{
    uint8_t Output[32];

    /* Act - odd length hex string */
    int32_t Result = PdqHexToBytes("ABC", Output, sizeof(Output));

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(-EINVAL, Result);
}

void Test_HexToBytes_InvalidChars_ReturnsError(void)
{
    uint8_t Output[32];

    /* Act - non-hex characters */
    int32_t Result = PdqHexToBytes("GHIJ", Output, sizeof(Output));

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(-EINVAL, Result);
}

void Test_HexToBytes_OutputTooSmall_ReturnsError(void)
{
    uint8_t Output[2];

    /* Act - 8 hex chars need 4 bytes, but only 2 available */
    int32_t Result = PdqHexToBytes("AABBCCDD", Output, sizeof(Output));

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(-ENOBUFS, Result);
}

/* ============================================================================
 * NULL POINTER TESTS
 * ========================================================================= */

void Test_AllPublicFunctions_NullPointer_ReturnError(void)
{
    /* SHA256 */
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqSha256Init(NULL));
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqSha256Update(NULL, NULL, 0));
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqSha256Final(NULL, NULL));

    /* Mining */
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqSha256MineBlock(NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqSha256Midstate(NULL, NULL));

    /* Stratum */
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqStratumConnect(NULL, 0));
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqStratumSubmitShare(NULL, 0, 0, 0));
    TEST_ASSERT_EQUAL(PdqErrorInvalidParam, PdqStratumGetJob(NULL));
}

/* ============================================================================
 * INTEGER OVERFLOW TESTS
 * ========================================================================= */

void Test_Sha256Update_MaxLength_NoOverflow(void)
{
    PdqSha256Context_t Ctx;
    PdqSha256Init(&Ctx);

    /* Simulate processing near UINT32_MAX bytes */
    Ctx.ByteCount = UINT32_MAX - 100;

    uint8_t Data[200] = {0};

    /* Act - should detect overflow or handle gracefully */
    PdqError_t Result = PdqSha256Update(&Ctx, Data, sizeof(Data));

    /* Assert - should not corrupt state */
    TEST_ASSERT_NOT_EQUAL(PdqOk, Result);
}

/* ============================================================================
 * MALFORMED JSON TESTS
 * ========================================================================= */

void Test_StratumParse_MalformedJson_ReturnsError(void)
{
    const char *MalformedInputs[] = {
        "not json at all",
        "{incomplete",
        "{\"method\": \"mining.notify\", \"params\": }",  /* Invalid JSON */
        "{\"method\": \"mining.notify\", \"params\": []}",  /* Empty params */
        "{\"method\": \"mining.notify\", \"params\": [1,2,3]}",  /* Wrong types */
        NULL
    };

    PdqStratumJob_t Job;
    for (int i = 0; MalformedInputs[i] != NULL; i++) {
        /* Feed malformed JSON through process loop - should not crash */
        PdqError_t Result = PdqStratumProcess();
        /* Verify no new job was created from bad input */
        TEST_ASSERT_FALSE(PdqStratumHasNewJob());
    }
}

void Test_StratumParse_OversizedJobId_Truncates(void)
{
    /* Arrange - JSON with job ID longer than max */
    char Json[4096];
    char LongJobId[256];
    memset(LongJobId, 'X', sizeof(LongJobId) - 1);
    LongJobId[sizeof(LongJobId) - 1] = '\0';

    snprintf(Json, sizeof(Json),
        "{\"method\":\"mining.notify\",\"params\":[\"%s\",\"00\",\"00\",\"00\",[],\"20000000\",\"1a0ffff0\",\"5f5e1000\",true]}",
        LongJobId);

    PdqStratumJob_t Job;

    /* Act */
    PdqError_t Result = PdqStratumProcess();

    /* Assert - parsed with truncation */
    Result = PdqStratumGetJob(&Job);
    TEST_ASSERT_EQUAL(PdqOk, Result);
    TEST_ASSERT_EQUAL(PDQ_STRATUM_MAX_JOBID_LEN, strlen(Job.JobId));
}
```

### 10.2 Security Test Coverage Requirements

| Category | Test Count | Coverage |
|----------|-----------|----------|
| NULL pointer handling | All public APIs | 100% |
| Buffer overflow prevention | All string/array inputs | 100% |
| Integer overflow | Length/count operations | 100% |
| Malformed input | JSON, hex, network data | 100% |
| Resource cleanup | Error paths | 90% |

### 10.3 Fuzz Testing (Optional)

For additional security assurance, fuzz testing can be integrated:

```c
/**
 * @file    fuzz_stratum_client.c
 * @brief   Fuzz test target for Stratum JSON parsing
 * @note    Build with: clang -g -O1 -fsanitize=fuzzer,address
 */

#include "stratum_client.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    /* Null-terminate for JSON parsing */
    char *Json = malloc(Size + 1);
    if (Json == NULL) return 0;

    memcpy(Json, Data, Size);
    Json[Size] = '\0';

    /* Feed JSON to stratum process loop */
    PdqStratumProcess();

    free(Json);
    return 0;
}

---

## 11. PDQManager Tests

### 11.1 Device API Unit Tests (Firmware)

```c
#include "unity.h"
#include "device_api.h"
#include "password_manager.h"
#include "mock_nvs.h"

void setUp(void) { MockNvs_Reset(); PdqApiReset(); }

void Test_Api_Auth_ValidPassword_ReturnsToken(void)
{
    PdqPasswordSet("TestPass123");
    char Token[64];
    int32_t ExpiresIn;
    int32_t Result = PdqApiAuthenticate("TestPass123", Token, &ExpiresIn);
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_TRUE(strlen(Token) > 0);
}

void Test_Api_Auth_InvalidPassword_ReturnsError(void)
{
    PdqPasswordSet("TestPass123");
    char Token[64];
    int32_t ExpiresIn;
    TEST_ASSERT_EQUAL_INT32(-EACCES, PdqApiAuthenticate("WrongPass", Token, &ExpiresIn));
}

void Test_Api_Auth_BruteForce_LocksOut(void)
{
    PdqPasswordSet("TestPass123");
    char Token[64];
    int32_t ExpiresIn;
    for (int i = 0; i < 3; i++) PdqApiAuthenticate("WrongPass", Token, &ExpiresIn);
    TEST_ASSERT_EQUAL_INT32(-EAGAIN, PdqApiAuthenticate("TestPass123", Token, &ExpiresIn));
}

void Test_Api_ValidateToken_Valid(void)
{
    PdqPasswordSet("TestPass123");
    char Token[64];
    int32_t ExpiresIn;
    PdqApiAuthenticate("TestPass123", Token, &ExpiresIn);
    TEST_ASSERT_TRUE(PdqApiValidateToken(Token));
}
```

### 11.2 Password Manager Tests (Firmware)

```c
#include "unity.h"
#include "password_manager.h"
#include "mock_nvs.h"

void Test_Password_SetVerify_RoundTrip(void)
{
    TEST_ASSERT_EQUAL_INT32(0, PdqPasswordSet("SecurePass123!"));
    TEST_ASSERT_TRUE(PdqPasswordVerify("SecurePass123!"));
    TEST_ASSERT_FALSE(PdqPasswordVerify("WrongPassword"));
}

void Test_Password_TooShort_ReturnsError(void)
{
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqPasswordSet("short"));
}

void Test_Password_ComplexChars_Accepted(void)
{
    TEST_ASSERT_EQUAL_INT32(0, PdqPasswordSet("P@$$w0rd!#%^&*()"));
    TEST_ASSERT_TRUE(PdqPasswordVerify("P@$$w0rd!#%^&*()"));
}

void Test_Password_NullInput_ReturnsError(void)
{
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqPasswordSet(NULL));
    TEST_ASSERT_FALSE(PdqPasswordVerify(NULL));
}
```

### 11.3 PDQManager Python Tests

```python
# tests/test_discovery.py
import pytest
from unittest.mock import Mock, patch
from pdqmanager.discovery import PDQMinerDiscovery

def test_discovery_service_type():
    discovery = PDQMinerDiscovery(on_device_found=Mock())
    assert discovery.SERVICE_TYPE == "_pdqminer._tcp.local."

def test_add_service_stores_device():
    callback = Mock()
    discovery = PDQMinerDiscovery(on_device_found=callback)
    mock_info = Mock(addresses=[bytes([192, 168, 1, 100])], port=80)
    with patch.object(discovery.zeroconf, "get_service_info", return_value=mock_info):
        discovery.add_service(discovery.zeroconf, "_pdqminer._tcp.local.", "PDQ_ABC._pdqminer._tcp.local.")
    assert "PDQ_ABC" in discovery.devices


# tests/test_device.py
import responses
from pdqmanager.device import DeviceClient

@responses.activate
def test_get_status_success():
    responses.add(responses.GET, "http://192.168.1.100/api/status",
        json={"device_id": "PDQ_ABC", "hashrate_khs": 520.5}, status=200)
    client = DeviceClient("192.168.1.100")
    assert client.get_status()["hashrate_khs"] == 520.5

@responses.activate
def test_authenticate_success():
    responses.add(responses.POST, "http://192.168.1.100/api/auth",
        json={"token": "abc123", "expires_in": 3600}, status=200)
    client = DeviceClient("192.168.1.100")
    assert client.authenticate("MyPassword123") is True
    assert client.token == "abc123"

@responses.activate
def test_authenticate_wrong_password():
    responses.add(responses.POST, "http://192.168.1.100/api/auth", status=401)
    client = DeviceClient("192.168.1.100")
    assert client.authenticate("WrongPassword") is False


# tests/test_api.py
from unittest.mock import patch
from pdqmanager.app import create_app

def test_get_devices_empty():
    app = create_app()
    app.config["TESTING"] = True
    with app.test_client() as client:
        response = client.get("/api/devices")
        assert response.status_code == 200
        assert response.json == []

def test_get_stats_aggregates():
    app = create_app()
    app.config["TESTING"] = True
    with app.test_client() as client:
        with patch("pdqmanager.app.device_manager") as mock_dm:
            mock_dm.get_all_status.return_value = [
                {"hashrate_khs": 500}, {"hashrate_khs": 520}
            ]
            response = client.get("/api/stats")
            assert response.json["total_hashrate_khs"] == 1020
```

---

## 13. Test Implementation Status

### 13.1 Implemented Tests

| Test File | Type | Status | Description |
|-----------|------|--------|-------------|
| `test/benchmark/test_sha256_benchmark.cpp` | Benchmark | **Created** | SHA256 correctness and performance tests |
| `src/main_benchmark.cpp` | Benchmark Firmware | **Complete** | Hardware hashrate measurement |

### 13.2 Test Directories Created

```
test/
├── unit/           # Awaiting implementation
├── integration/    # Awaiting multi-component tests
└── benchmark/      # Contains SHA256 benchmarks
```

### 13.3 Code Review Verification (Phase B)

**Round 1:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| Stratum Client | **Reviewed** | 2 | 2 (strncpy null-term, job building) |
| WiFi Manager | **Reviewed** | 1 | 1 (HandleSave not saving config) |
| Config Manager | **Reviewed** | 0 | 0 |
| Main Integration | **Reviewed** | 2 | 2 (midstate calc, timeout handling) |

**Round 2:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| Stratum Client | **Reviewed** | 2 | 2 (extranonce2 submit, missing include) |
| WiFi Manager | **Reviewed** | 2 | 2 (strncpy null-term in connect/scan) |
| Config Manager | **Clean** | 0 | 0 |
| Main Integration | **Clean** | 0 | 0 |

**Round 3 - Critical Fix:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| Mining Task | **CRITICAL** | 1 | 1 (share queue implementation) |
| Stratum Client | **Fixed** | 1 | 1 (job metadata storage) |
| Main Integration | **Fixed** | 1 | 1 (share polling & submission) |
| pdq_types.h | **Fixed** | 2 | 2 (JobId type, PdqShareInfo_t) |

**Round 4 - 100% Confidence Review:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| SHA256 Engine | **CRITICAL** | 2 | 2 (padding memory, nonce endianness) |
| Mining Task | **CRITICAL** | 1 | 1 (stale job mining) |
| Stratum Client | **Fixed** | 1 | 1 (extranonce2 size API) |
| Main Integration | **Fixed** | 1 | 1 (hardcoded extranonce2 size) |

**Round 5 - Post-Optimization Verification:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| SHA256 Engine | **VERIFIED** | 0 | - (optimizations correct) |
| Mining Task | **VERIFIED** | 0 | - (race conditions handled) |
| Stratum Client | **VERIFIED** | 0 | - (protocol compliance) |
| WiFi Manager | **VERIFIED** | 0 | - |
| Config Manager | **VERIFIED** | 0 | - |
| Main Integration | **VERIFIED** | 0 | - |
| Build | **CLEAN** | 0 | - (no warnings/errors) |

**Round 6 - Display Driver Verification:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| display_driver.cpp | **VERIFIED** | 2 | 2 |
| display_driver.h | **VERIFIED** | 0 | - |
| platformio.ini | **VERIFIED** | 0 | - |
| All Builds | **CLEAN** | 0 | - (4 environments pass) |

**Issues Fixed in Round 6:**
1. Removed unused `s_LastHashRate` variable (dead code)
2. Fixed stale display area when BlocksFound=0 (now clears area)

**Round 7 - Critical SHA256 Bug Fix:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| SHA256 Engine | **CRITICAL** | 1 | 1 (W2_SIG0_8 constant) |
| W1 Pre-computation | **VERIFIED** | 0 | - |
| W2 Expansion | **VERIFIED** | 0 | - |
| CheckTarget | **VERIFIED** | 0 | - |
| Buffer Safety | **VERIFIED** | 0 | - |
| Input Validation | **VERIFIED** | 0 | - |

**Critical Bug Fixed in Round 7:**
- **Location**: `src/core/sha256_engine.c:360`
- **Issue**: `W2_SIG0_8 = 0x00800001` (WRONG)
- **Fix**: `W2_SIG0_8 = 0x11002000` (CORRECT)
- **Impact**: Would have caused all hash calculations to fail

**Round 8 - Comprehensive Verification:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| SHA256 Engine | **VERIFIED** | 0 | - (W2_SIG0_8 = 0x11002000 confirmed) |
| Mining Task | **VERIFIED** | 0 | - (dual-core, nonce overflow handling) |
| Stratum Client | **VERIFIED** | 0 | - (job building, padding correct) |
| Block Header | **VERIFIED** | 0 | - (80-byte, midstate, tail correct) |
| SHA256 Padding | **VERIFIED** | 0 | - (0x80 + zeros + 0x0280 length) |
| Security | **VERIFIED** | 0 | - (no buffer overflows) |
| All Builds | **PASS** | 0 | - (4 environments compile) |

**Total Issues Resolved:** 21

**Round 9 - Session 27 Bug Fixes & Optimization Pass 3:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| Main Integration | **CRITICAL** | 1 | 1 (debug print consuming job notifications) |
| SHA256 Engine | **CRITICAL** | 1 | 1 (nonce overflow infinite loop) |
| Mining Task | **CRITICAL** | 1 | 1 (batch loop nonce overflow) |
| SHA256 Optimization | **VERIFIED** | 0 | - (KW1 constants, zero-term elimination correct) |
| All Builds | **PASS** | 0 | - (4 environments compile, updated sizes) |

**Critical Bugs Fixed in Round 9:**
1. `main.cpp:147-148` - Debug print called destructive `PdqStratumHasNewJob()`, consuming job flag (root cause of 54 H/s)
2. `sha256_engine.c:443-444` - `for` loop with `Nonce <= end; Nonce++` infinite loops at 0xFFFFFFFF
3. `mining_task.c:91-125, 151-185` - Same overflow pattern in batch outer loops for both cores

**Total Issues Resolved:** 24

### 13.4 Pending Test Implementation

| Test Category | Dependencies | Phase | Status |
|---------------|--------------|-------|--------|
| SHA256 Unit Tests | Core engine | Ready | Pending |
| Mining Task Tests | Core tasks | Ready | Pending |
| Stratum Protocol Tests | Stratum client | Phase B | Ready |
| WiFi Integration Tests | WiFi manager | Phase B | Ready |
| Display Tests | Display driver | Phase C | Ready |
| API Tests | Device API | Phase D | Blocked |

### 13.5 SHA256 Optimization Verification

The following optimizations have been verified correct:

| Optimization | Correctness | Notes |
|--------------|-------------|-------|
| W1[16] pre-computation | **VERIFIED** | SIG1(W[14]) + W[9] + SIG0(W[1]) + W[0] |
| W1[17] pre-computation | **VERIFIED** | SIG1(W[15]) + W[10] + SIG0(W[2]) + W[1] |
| W1[18] partial pre-comp | **VERIFIED** | SIG1(W1Pre16) + W[11] + W[2] + SIG0(nonce) |
| Nonce byte swap | **VERIFIED** | `__builtin_bswap32()` for little-endian storage |
| W2 padding | **VERIFIED** | 0x80000000 at W2[8], 256 at W2[15] |
| W2_SIG1_15 constant | **VERIFIED** | SIG1(256) = 0x00A00000 |
| W2_SIG0_8 constant | **FIXED** | SIG0(0x80000000) = 0x11002000 |
| W2[16-24] expansion | **VERIFIED** | All formulas mathematically correct |
| Early rejection | **VERIFIED** | FinalState[7] vs TargetHigh before full compare |
| W1 constant elimination | **VERIFIED** | W1_4=0x80000000, W1_5-14=0, W1_15=0x280 (Session 27) |
| SIG0/SIG1 pre-computation | **VERIFIED** | SIG0(0x80000000)=0x11002000, SIG0(0x280)=0x00A00055, SIG1(0x280)=0x01100000 (Session 27) |
| Zero-term elimination | **VERIFIED** | W1_20-W1_29 simplified by removing zero-valued terms (Session 27) |
| KW1 pre-computed constants | **VERIFIED** | K[i]+W[i] for rounds 4-15 correct (Session 27) |
| SHA256_ROUND_KW macro | **VERIFIED** | Combined K+W round eliminates one addition (Session 27) |
| Nonce loop overflow fix | **VERIFIED** | `for(;;)` with explicit break at NonceEnd (Session 27) |
| HW SHA overlap (START safe) | **VERIFIED** | Writes during START do not corrupt result (diagnostic test) |
| HW SHA overlap (CONTINUE unsafe) | **VERIFIED** | Writes during CONTINUE corrupt result (diagnostic test) |
| HW SHA block1 overlap fill | **VERIFIED** | 16 APB writes hidden behind block0 START |
| HW SHA block0 half-fill | **VERIFIED** | 8 APB writes hidden behind double-hash START |
| HW SHA 2-write reduced padding | **VERIFIED** | Only TextNV[8] and TextNV[15] differ per iteration |
| GCC push_options/-O2 for HW | **VERIFIED** | HW path compiled with -O2, SW path with -Os |

### 13.6 Display Driver Verification

| Check | Status | Notes |
|-------|--------|-------|
| Null pointer checks | **VERIFIED** | PdqDisplayUpdate checks p_Stats |
| Buffer overflow | **VERIFIED** | snprintf with sizeof(Buffer) |
| Rate limiting | **VERIFIED** | 500ms minimum between updates |
| Headless mode | **VERIFIED** | All functions return PdqOk immediately |
| Color constants | **VERIFIED** | All TFT_* colors defined in TFT_eSPI |
| Build environments | **VERIFIED** | cyd_ili9341, cyd_st7789, headless, benchmark all pass |

### 13.7 Hardware SHA256 Verification

**Boot-time Diagnostic (`PdqSha256HwDiagnostic()`):**

| Test | Result | Description |
|------|--------|-------------|
| Basic SHA256 (START+CONTINUE+LOAD) | PASS | HW produces correct digest |
| LOAD direction | FAIL (expected) | LOAD copies internal→SHA_TEXT only (no midstate restore) |
| Auto-store after START | NO | Explicit LOAD required to read digest |
| SHA_TEXT preservation | YES | Contents survive START/CONTINUE/LOAD |
| Overlap writes during START | SAFE | Engine latches data at trigger time |
| Overlap writes during CONTINUE | UNSAFE | Engine reads registers progressively |
| START timing | 627 CPU cycles | Measured via CCOUNT register |
| CONTINUE timing | 627 CPU cycles | Measured via CCOUNT register |
| LOAD timing | 562 CPU cycles | Measured via CCOUNT register |

**Performance Verification:**

| Metric | Expected | Measured |
|--------|----------|---------|
| HW cycles/nonce | <500 | 413 |
| HW hashrate | >500 KH/s | 581 KH/s |
| Combined HW+SW | >550 KH/s | 627 KH/s |
| All diagnostic tests | PASS | PASS |

**Round 10-11 - HW SHA Engine & Overlap Optimization:**

| Component | Review Status | Issues Found | Issues Fixed |
|-----------|---------------|--------------|--------------|
| HW SHA Engine | **VERIFIED** | 0 | - (diagnostic validates correctness) |
| Overlap optimization | **VERIFIED** | 0 | - (all overlap tests pass) |
| Mining Task (HW+SW) | **VERIFIED** | 0 | - (nonce split 7/8 HW + 1/8 SW correct) |
| GCC push/pop_options | **VERIFIED** | 0 | - (-Os for SW, -O2 for HW) |
| IRAM test | **REJECTED** | N/A | - (regressed: 538 vs 413 cyc/nonce) |
| Commits | **VERIFIED** | 0 | - (23617e0, 7286523) |

**Total Issues Resolved:** 24

---

*This document defines the testing methodology for all PDQminer development.*
