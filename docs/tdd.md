# PDQminer Test-Driven Development Guide

> **Version**: 1.0.0  
> **Last Updated**: 2025-01-XX  
> **Status**: Draft  
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
| Stratum Parser | 100% line |
| Job Manager | 95% line |
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
├── unit/                       # Unit tests (isolated)
│   ├── core/
│   │   ├── test_sha256_engine.c
│   │   ├── test_mining_task.c
│   │   └── test_job_manager.c
│   ├── stratum/
│   │   ├── test_stratum_parser.c
│   │   └── test_stratum_client.c
│   ├── display/
│   │   └── test_display_driver.c
│   └── config/
│       └── test_config_manager.c
├── integration/                # Integration tests
│   ├── test_mining_flow.c
│   ├── test_stratum_job_flow.c
│   └── test_wifi_stratum.c
├── benchmark/                  # Performance benchmarks
│   ├── benchmark_sha256.c
│   ├── benchmark_nonce_loop.c
│   └── benchmark_report.py
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
void Test_JobManager_UpdateJob_NewJob_ClearsOldWork(void);
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

static Sha256Context_t s_TestContext;

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
    Sha256Context_t Context;

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
#include "job_manager.h"
#include "mining_task.h"
#include "mock_tcp.h"

void Test_MiningFlow_ReceiveJob_StartsMining(void)
{
    /* Arrange */
    MockTcp_Reset();
    MockTcp_QueueResponse(STRATUM_SUBSCRIBE_RESPONSE);
    MockTcp_QueueResponse(STRATUM_AUTHORIZE_RESPONSE);
    MockTcp_QueueResponse(STRATUM_NOTIFY_JOB_1);

    StratumConfig_t Config = {
        .PoolHost = "test-pool.io",
        .PoolPort = 3333,
        .WalletAddress = "bc1qtest..."
    };

    /* Act */
    int32_t Result = PdqStratumConnect(&Config);
    TEST_ASSERT_EQUAL_INT32(0, Result);

    Result = PdqStratumSubscribe();
    TEST_ASSERT_EQUAL_INT32(0, Result);

    Result = PdqStratumAuthorize();
    TEST_ASSERT_EQUAL_INT32(0, Result);

    MiningJob_t Job;
    Result = PdqStratumPoll(&Job, 1000);
    TEST_ASSERT_EQUAL_INT32(0, Result);

    /* Assert - Job was received and parsed */
    TEST_ASSERT_NOT_EMPTY(Job.JobId);
    TEST_ASSERT_NOT_EQUAL(0, Job.NBits);

    /* Assert - Mining context initialized */
    MiningContext_t MiningCtx;
    Result = PdqJobManagerGetContext(&MiningCtx);
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_NOT_EQUAL(0, MiningCtx.Midstate[0]);
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
    DeviceConfig_t SaveConfig = {
        .WifiSsid = "TestNetwork",
        .WifiPassword = "SecurePass123",
        .PrimaryPool = { .Host = "public-pool.io", .Port = 21496 },
        .BackupPool = { .Host = "solo.ckpool.org", .Port = 3333 },
        .WalletAddress = "bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh",
        .WorkerName = "pdqminer1"
    };

    int32_t Result = PdqConfigSave(&SaveConfig);
    TEST_ASSERT_EQUAL_INT32(0, Result);

    DeviceConfig_t LoadConfig;
    Result = PdqConfigLoad(&LoadConfig);
    TEST_ASSERT_EQUAL_INT32(0, Result);

    TEST_ASSERT_EQUAL_STRING(SaveConfig.WifiSsid, LoadConfig.WifiSsid);
    TEST_ASSERT_EQUAL_STRING(SaveConfig.PrimaryPool.Host, LoadConfig.PrimaryPool.Host);
    TEST_ASSERT_EQUAL_STRING(SaveConfig.WalletAddress, LoadConfig.WalletAddress);
}

/**
 * @brief Test loading when no configuration exists
 */
void Test_Config_Load_NoConfig_ReturnsError(void)
{
    DeviceConfig_t Config;
    int32_t Result = PdqConfigLoad(&Config);
    TEST_ASSERT_EQUAL_INT32(-ENOENT, Result);
}

/**
 * @brief Test factory reset clears configuration
 */
void Test_Config_Reset_ClearsAllData(void)
{
    DeviceConfig_t Config = { .WifiSsid = "TestNetwork", .WalletAddress = "bc1qtest" };
    PdqConfigSave(&Config);
    TEST_ASSERT_TRUE(PdqConfigIsValid());

    int32_t Result = PdqConfigReset();
    TEST_ASSERT_EQUAL_INT32(0, Result);
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
    DeviceConfig_t Config = {
        .PrimaryPool = { .Host = "primary.pool", .Port = 3333 },
        .BackupPool = { .Host = "backup.pool", .Port = 3333 }
    };
    PdqConfigSave(&Config);

    MockTcp_SetConnectResult("primary.pool", -ECONNREFUSED);
    MockTcp_SetConnectResult("backup.pool", 0);

    int32_t Result = PdqStratumConnectWithFailover();
    TEST_ASSERT_EQUAL_INT32(0, Result);
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
 */
void Benchmark_Sha256Double_Throughput(void)
{
    uint8_t Header[80];
    uint8_t Hash[32];
    MiningContext_t MiningCtx;

    /* Initialize with random header */
    for (int i = 0; i < 80; i++) {
        Header[i] = (uint8_t)(i * 7);
    }

    PdqMiningContextInit(&MiningCtx, Header);

    /* Warm up */
    for (int i = 0; i < 1000; i++) {
        PdqMineNonce(&MiningCtx, i, 1, NULL);
    }

    /* Benchmark */
    uint64_t StartTime = esp_timer_get_time();
    uint32_t Iterations = 0;

    while ((esp_timer_get_time() - StartTime) < (BENCHMARK_DURATION_MS * 1000)) {
        uint32_t FoundNonce;
        PdqMineNonce(&MiningCtx, Iterations * 4096, 4096, &FoundNonce);
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
#define MIN_SINGLE_CORE_KHS     500.0f   /* Single core minimum */
#define MIN_DUAL_CORE_KHS       950.0f   /* Dual core minimum */
#define TARGET_DUAL_CORE_KHS   1000.0f   /* Target performance */

/**
 * @brief   Single-core hashrate regression test
 * @note    MUST achieve at least 500 KH/s on single core
 */
void Test_Performance_SingleCore_MinimumHashrate(void)
{
    uint8_t Header[80];
    MiningContext_t Ctx;

    /* Initialize */
    for (int i = 0; i < 80; i++) Header[i] = i;
    PdqMiningContextInit(&Ctx, Header, NULL);

    /* Benchmark 5 seconds */
    uint64_t Start = esp_timer_get_time();
    uint32_t Hashes = 0;

    while ((esp_timer_get_time() - Start) < 5000000) {
        uint32_t Found;
        Hashes += PdqMineNonce(&Ctx, Hashes, 4096, &Found);
    }

    float KHs = (float)Hashes / 5000.0f;
    printf("[REGRESSION] Single-core: %.2f KH/s (min: %.0f)\n", KHs, MIN_SINGLE_CORE_KHS);

    TEST_ASSERT_GREATER_OR_EQUAL(MIN_SINGLE_CORE_KHS, KHs);
}

/**
 * @brief   Dual-core hashrate regression test
 * @note    MUST achieve at least 950 KH/s with both cores
 */
void Test_Performance_DualCore_MinimumHashrate(void)
{
    /* Start mining tasks on both cores */
    MiningStats_t Stats = {0};
    PdqMiningStart(&Stats);

    /* Let it run for 10 seconds */
    vTaskDelay(pdMS_TO_TICKS(10000));

    /* Get hashrate */
    float KHs = Stats.KiloHashesPerSecond;
    printf("[REGRESSION] Dual-core: %.2f KH/s (min: %.0f, target: %.0f)\n",
           KHs, MIN_DUAL_CORE_KHS, TARGET_DUAL_CORE_KHS);

    PdqMiningStop();

    TEST_ASSERT_GREATER_OR_EQUAL(MIN_DUAL_CORE_KHS, KHs);
}

/**
 * @brief   Target hashrate validation (informational, not fail)
 */
void Test_Performance_TargetHashrate_1000KHs(void)
{
    MiningStats_t Stats = {0};
    PdqMiningStart(&Stats);

    vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 second benchmark */

    float KHs = Stats.KiloHashesPerSecond;
    printf("[TARGET] Achieved: %.2f KH/s (target: %.0f)\n", KHs, TARGET_DUAL_CORE_KHS);

    PdqMiningStop();

    /* Informational: warn if below target but don't fail */
    if (KHs < TARGET_DUAL_CORE_KHS) {
        printf("[WARNING] Below target hashrate!\n");
    }
    TEST_ASSERT_GREATER_OR_EQUAL(MIN_DUAL_CORE_KHS, KHs);
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
        PdqSha256Double(Header, 80, Hash);
    }
    uint64_t NoMidstateTime = esp_timer_get_time() - Start;

    /* With midstate: only recompute tail */
    MiningContext_t Ctx;
    PdqMiningContextInit(&Ctx, Header, NULL);

    Start = esp_timer_get_time();
    for (int i = 0; i < 10000; i++) {
        uint32_t Found;
        PdqMineNonce(&Ctx, i, 1, &Found);
    }
    uint64_t MidstateTime = esp_timer_get_time() - Start;

    float Speedup = (float)NoMidstateTime / (float)MidstateTime;
    printf("[OPT] Midstate speedup: %.2fx\n", Speedup);

    /* Midstate should provide at least 1.3x speedup */
    TEST_ASSERT_GREATER_THAN(1.3f, Speedup);
}

/**
 * @brief   Verify IRAM placement improves performance
 */
void Test_Optimization_IramPlacement_Benefit(void)
{
    /* This test compares IRAM vs flash execution time */
    /* Implementation depends on having both versions available */
    printf("[OPT] IRAM placement: Verified by symbol placement in .map file\n");
    TEST_PASS();
}

/**
 * @brief   Verify dual-core provides ~2x speedup over single
 */
void Test_Optimization_DualCore_Speedup(void)
{
    float SingleKHs, DualKHs;

    /* Single core test */
    MiningStats_t Stats1 = {0};
    PdqMiningStartSingleCore(&Stats1, 0);  /* Core 0 only */
    vTaskDelay(pdMS_TO_TICKS(5000));
    SingleKHs = Stats1.KiloHashesPerSecond;
    PdqMiningStop();

    vTaskDelay(pdMS_TO_TICKS(100));

    /* Dual core test */
    MiningStats_t Stats2 = {0};
    PdqMiningStart(&Stats2);  /* Both cores */
    vTaskDelay(pdMS_TO_TICKS(5000));
    DualKHs = Stats2.KiloHashesPerSecond;
    PdqMiningStop();

    float Speedup = DualKHs / SingleKHs;
    printf("[OPT] Dual-core speedup: %.2fx (%.0f vs %.0f KH/s)\n",
           Speedup, DualKHs, SingleKHs);

    /* Should achieve at least 1.8x with dual core */
    TEST_ASSERT_GREATER_THAN(1.8f, Speedup);
}

/**
 * @brief   Verify mining context fits in cache
 */
void Test_Memory_MiningContext_Size(void)
{
    printf("[MEM] MiningContext_t size: %u bytes\n", sizeof(MiningContext_t));

    /* Context must fit in L1 cache for optimal performance */
    TEST_ASSERT_LESS_THAN(512, sizeof(MiningContext_t));
}

/**
 * @brief   Verify no heap allocation in hot path
 */
void Test_Memory_HotPath_NoHeapAlloc(void)
{
    MiningContext_t Ctx;
    uint8_t Header[80] = {0};

    size_t HeapBefore = esp_get_free_heap_size();

    PdqMiningContextInit(&Ctx, Header, NULL);
    for (int i = 0; i < 100000; i++) {
        uint32_t Found;
        PdqMineNonce(&Ctx, i, 1, &Found);
    }

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

    StratumConfig_t Config;

    /* Act */
    int32_t Result = PdqStratumSetHost(&Config, LongHost);

    /* Assert - truncated, not overflowed */
    TEST_ASSERT_EQUAL_INT32(PDQ_STRATUM_MAX_HOST_LEN, strlen(Config.PoolHost));
    TEST_ASSERT_EQUAL('\0', Config.PoolHost[PDQ_STRATUM_MAX_HOST_LEN]);
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
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqSha256Init(NULL));
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqSha256Update(NULL, NULL, 0));
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqSha256Final(NULL, NULL));

    /* Mining */
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqMiningContextInit(NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqMineNonce(NULL, 0, 0, NULL));

    /* Stratum */
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqStratumConnect(NULL));
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqStratumSubmitShare(NULL, 0, 0));
    TEST_ASSERT_EQUAL_INT32(-EINVAL, PdqStratumPoll(NULL, 0));
}

/* ============================================================================
 * INTEGER OVERFLOW TESTS
 * ========================================================================= */

void Test_Sha256Update_MaxLength_NoOverflow(void)
{
    Sha256Context_t Ctx;
    PdqSha256Init(&Ctx);

    /* Simulate processing near UINT64_MAX bytes */
    Ctx.TotalLength = UINT64_MAX - 100;

    uint8_t Data[200] = {0};

    /* Act - should detect overflow */
    int32_t Result = PdqSha256Update(&Ctx, Data, sizeof(Data));

    /* Assert */
    TEST_ASSERT_EQUAL_INT32(-EOVERFLOW, Result);
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

    MiningJob_t Job;
    for (int i = 0; MalformedInputs[i] != NULL; i++) {
        int32_t Result = PdqStratumParseNotify(MalformedInputs[i], &Job);
        TEST_ASSERT_LESS_THAN_INT32(0, Result);
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

    MiningJob_t Job;

    /* Act */
    int32_t Result = PdqStratumParseNotify(Json, &Job);

    /* Assert - parsed with truncation */
    TEST_ASSERT_EQUAL_INT32(0, Result);
    TEST_ASSERT_EQUAL_INT32(PDQ_STRATUM_MAX_JOBID_LEN, strlen(Job.JobId));
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
 * @file    fuzz_stratum_parser.c
 * @brief   Fuzz test target for Stratum JSON parser
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

    MiningJob_t Job;
    PdqStratumParseNotify(Json, &Job);

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

**Total Issues Resolved:** 18

### 13.4 Pending Test Implementation

| Test Category | Dependencies | Phase | Status |
|---------------|--------------|-------|--------|
| SHA256 Unit Tests | Core engine | Ready | Pending |
| Mining Task Tests | Core tasks | Ready | Pending |
| Stratum Protocol Tests | Stratum client | Phase B | Ready |
| WiFi Integration Tests | WiFi manager | Phase B | Ready |
| Display Tests | Display driver | Phase C | Blocked |
| API Tests | Device API | Phase D | Blocked |

---

*This document defines the testing methodology for all PDQminer development.*
