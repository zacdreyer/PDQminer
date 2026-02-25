# PDQminer Software Design Document (SDD)

> **Version**: 1.2.0
> **Last Updated**: 2025-02-14
> **Status**: Active
> **Owner**: PDQminer Team

---

## 1. Introduction

### 1.1 Purpose

This Software Design Document (SDD) describes the architecture, components, interfaces, and design decisions for PDQminer - a high-performance open-source Bitcoin mining firmware for ESP32 microcontrollers.

### 1.2 Scope

PDQminer targets ESP32-D0 class devices with the goal of achieving >1000 KH/s SHA256d throughput while maintaining:
- Full open-source transparency
- Clean, maintainable architecture
- Comprehensive test coverage
- Easy board portability

### 1.3 Definitions and Acronyms

| Term | Definition |
|------|------------|
| SHA256d | Double SHA256: `SHA256(SHA256(data))` |
| Midstate | Partially computed SHA256 state (first 64 bytes) |
| Stratum | Bitcoin mining pool communication protocol |
| Nonce | 32-bit value iterated to find valid block hash |
| CYD | Cheap Yellow Display (ESP32-2432S028R board) |
| HPNA | High Performance NerdMiner Alternative (codename) |
| Project Kilo | Internal codename for reference closed-source firmware |

### 1.4 References

| Document | Description |
|----------|-------------|
| `coding-standards.md` | PDQminer coding conventions |
| `documentation-standards.md` | Documentation format guide |
| `tdd.md` | Test-Driven Development guide |
| `agent-memory.md` | Project context and history |

---

## 2. System Overview

### 2.1 System Context

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Mining Pool                                  │
│                    (public-pool.io:21496)                           │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ Stratum Protocol
                           │ (TCP/JSON-RPC)
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        PDQminer Firmware                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐ │
│  │   WiFi      │  │  Stratum    │  │   Mining    │  │   Display  │ │
│  │   Manager   │  │  Client     │  │   Engine    │  │   Driver   │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    ESP32-2432S028R Hardware                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐ │
│  │  WiFi       │  │  Dual-Core  │  │  SPI Flash  │  │  TFT LCD   │ │
│  │  Radio      │  │  240MHz     │  │  4MB        │  │  320x240   │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Design Goals

| Priority | Goal | Target |
|----------|------|--------|
| P0 | Hashrate | >1000 KH/s on ESP32-D0 |
| P0 | Stability | No crashes, proper WDT handling |
| P1 | Maintainability | Clean architecture, full documentation |
| P1 | Testability | >90% code coverage on critical paths |
| P2 | User Experience | Simple setup, informative display |
| P2 | Portability | Easy board adaptation |

### 2.3 Performance Analysis (Reference Firmware)

Analysis of Project Kilo's performance progression reveals optimization strategy:

| Version | Hashrate | Likely Optimization |
|---------|----------|---------------------|
| v0.4.03 | 92 KH/s | Baseline |
| v1.1.02 | 224 KH/s | Basic loop optimization |
| v1.1.03 | 340 KH/s | Message schedule caching |
| v1.2.01 | 375 KH/s | Reduced WDT overhead |
| v1.3.01 | 412 KH/s | Core pinning |
| v1.6.03 | 483 KH/s | Midstate optimization |
| v1.7.02 | 1010 KH/s | **Full SHA256 unroll + nonce-only update** |
| v1.8.24 | 1035 KH/s | Assembly optimization |

**Key Insight**: The 2x jump from 483 KH/s to 1010 KH/s indicates a fundamental algorithm change - likely full SHA256 round unrolling with nonce-only recomputation.

---

## 3. Architecture

### 3.1 Repository Structure

```
PDQminer/
├── docs/                       # Documentation
│   ├── agents/
│   │   ├── agent-memory.md     # Agent context persistence
│   │   └── agent-prompts.md    # Common prompts for agents
│   ├── assets/
│   │   └── logo-pdqminer.png   # Project logo
│   ├── coding-standards.md
│   ├── documentation-standards.md
│   ├── sdd.md
│   └── tdd.md
├── src/                        # Firmware Source (flat, no firmware/ wrapper)
│   ├── main.cpp                # Application entry point (Arduino framework)
│   ├── main_benchmark.cpp      # Standalone benchmark firmware
│   ├── pdq_types.h             # Common type definitions
│   ├── core/                   # Core mining logic
│   │   ├── sha256_engine.c
│   │   ├── sha256_engine.h
│   │   ├── mining_task.c
│   │   └── mining_task.h
│   ├── stratum/                # Pool communication
│   │   ├── stratum_client.c
│   │   └── stratum_client.h
│   ├── network/                # WiFi and network
│   │   ├── wifi_manager.cpp    # C++ for Arduino WebServer/IotWebConf
│   │   └── wifi_manager.h
│   ├── display/                # Display driver (TFT_eSPI)
│   │   ├── display_driver.cpp  # C++ for TFT_eSPI library
│   │   └── display_driver.h
│   ├── config/                 # Configuration management
│   │   ├── config_manager.c
│   │   └── config_manager.h
│   ├── api/                    # Device REST API (stub)
│   │   ├── device_api.c
│   │   └── device_api.h
│   └── hal/                    # Hardware abstraction
│       ├── board_hal.c
│       └── board_hal.h
├── test/                       # Test suite
│   ├── unit/                   # Unit tests (pending)
│   ├── integration/            # Integration tests (pending)
│   └── benchmark/
│       └── test_sha256_benchmark.cpp
├── boards/                     # Board-specific configs (placeholder)
├── tools/                      # Build utilities (placeholder)
│   ├── pdqflasher/
│   └── pdqmanager/
├── platformio.ini              # PlatformIO configuration
├── serial_monitor.py           # Serial monitoring helper
├── LICENSE
└── README.md
```

### 3.2 Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                          Application Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ Screen Mgr   │  │ Config Web   │  │ Button Handler       │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                          Service Layer                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ Job Manager  │  │ Stratum      │  │ Statistics           │  │
│  │              │◀─│ Client       │  │ Collector            │  │
│  └──────┬───────┘  └──────────────┘  └──────────────────────┘  │
│         │                                                        │
│  ┌──────▼───────┐                                               │
│  │ Mining Task  │  (Pinned to Core 0)                           │
│  │ Mining Task  │  (Pinned to Core 1)                           │
│  └──────┬───────┘                                               │
└─────────┼───────────────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────────────┐
│                           Core Layer                             │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    SHA256 Engine                          │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │   │
│  │  │ Midstate    │  │ Nonce Loop  │  │ Target Check    │   │   │
│  │  │ Computer    │  │ (Unrolled)  │  │                 │   │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────┘   │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Hardware Abstraction Layer                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ WiFi HAL     │  │ Display HAL  │  │ Storage HAL          │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Component Design

### 4.1 SHA256 Engine

**Purpose**: High-performance SHA256d computation optimized for Bitcoin mining.

**Key Optimizations**:

1. **Midstate Precomputation**
   - First 64 bytes of block header are constant per job
   - Compute and cache midstate once per job
   - Only recompute last 16 bytes per nonce

2. **Nonce-Only Update**
   - Block header bytes 76-79 contain nonce
   - Only update W[3] in message schedule per iteration
   - Recompute rounds 3-15 partially, then full 16-63

3. **Full Round Unrolling**
   - Eliminate loop overhead (64 iterations → inline)
   - Macro-based round expansion
   - Compiler can optimize register allocation

4. **Memory Layout (ESP32-Specific)**
   - Place hot functions in IRAM (32KB limit)
   - Place constants (K[64]) in DRAM, not flash
   - Align structures to 32-bit boundaries
   - Minimize cache misses with data locality

5. **Register Optimization**
   - Use `register` hints for working variables
   - Keep state variables (a-h) in registers across rounds
   - Minimize memory load/store operations
   - Use local copies to avoid volatile reads in inner loop

6. **Compiler Optimization Settings**
   - `-O2` optimization level for release builds (not -O3, to avoid code bloat)
   - `-ffast-math` enabled (integer-only hot path unaffected)

7. **Branch Elimination**
   - No conditional branches in SHA256 rounds (constant-time)
   - Early-exit only on target check (after double hash)
   - Use bitwise operations instead of conditionals where possible

8. **ESP32 Hardware SHA256 Acceleration**
   - ESP32-D0 has a dedicated SHA256 hardware peripheral at `SHA_TEXT_BASE` (0x3FF03000)
   - HW SHA processes one 64-byte block via START (first block) or CONTINUE (subsequent blocks)
   - Timing: START=627 CPU cycles, CONTINUE=627 CPU cycles, LOAD=562 CPU cycles
   - HW mining achieves **~650 KH/s** (~369 CPU cycles/nonce) vs ~46 KH/s for SW per core
   - **Midstate caching is NOT possible on ESP32-D0**: No `SHA_H_BASE` register exists (only on ESP32-S2/S3/C3). LOAD copies digest from internal state to SHA_TEXT (read-only direction). Espressif docs: "not possible to restore a SHA digest state into the engine."
   - Each double-SHA256 requires: START (block0) + CONTINUE (block1) + LOAD (intermediate) + START (double-hash) + LOAD (final)
   - **START→CONTINUE chaining**: Writing to CONTINUE_REG while START is busy causes the engine to auto-chain atomically (undocumented, verified by diagnostic). Eliminates idle gap between START and CONTINUE.

9. **Overlapped Register Writes**
   - Writes to SHA_TEXT during START are **safe** (engine latches data at trigger)
   - Writes to SHA_TEXT during CONTINUE are **unsafe** (engine reads progressively)
   - Optimization: Fill next block's SHA_TEXT registers while current block is being hashed
   - Block1 fill (16 APB writes) overlapped with Block0 START
   - Reduced double-hash overlap: only block0[8:15] via `HwShaFillBlock0Upper()` (saves 8 writes vs full block0 fill since LOAD overwrites [0:7])
   - Padding overlap during intermediate LOAD (2 writes hidden behind LOAD latency)
   - 2-write reduced padding (only TextNV[8] and TextNV[15] differ between iterations)

10. **GCC Optimization Split**
    - SW mining code compiled with `-Os` (size-optimized, frees IRAM/cache for HW path)
    - HW mining code compiled with `-O2` via GCC `push_options`/`pop_options` pragmas
    - IRAM tested for HW path but regressed (538 vs 413 cyc/nonce) - flash cache already efficient

9. **Cache Optimization**
   - K constants in contiguous DRAM array (cache-friendly)
   - Mining context fits in L1 data cache
   - Avoid cache thrashing between cores (separate context instances)

**Data Structures**:

```c
/**
 * @brief SHA256 computation context
 * @note  Fields match pdq_types.h PdqSha256Context_t
 */
typedef struct {
    uint32_t State[8];              /**< SHA256 state (H0-H7) */
    uint8_t  Buffer[64];            /**< Input buffer for partial blocks */
    uint32_t ByteCount;             /**< Total bytes processed */
} PdqSha256Context_t;

/**
 * @brief Mining job context for nonce iteration
 * @note  Aligned to 4-byte boundary for ESP32 word access
 * @note  Passed to PdqSha256MineBlock() for optimized mining
 */
typedef struct __attribute__((aligned(4))) {
    uint8_t  Midstate[32];          /**< Precomputed state after first 64 bytes */
    uint8_t  BlockTail[64];         /**< Last 64 bytes with SHA256 padding */
    uint32_t NonceStart;            /**< First nonce to test */
    uint32_t NonceEnd;              /**< Last nonce to test */
    uint32_t Target[8];             /**< Difficulty target for comparison */
    char     JobId[65];             /**< Stratum job ID (string) */
    uint32_t Extranonce2;           /**< Extranonce2 value used */
    uint32_t NTime;                 /**< Block timestamp */
} PdqMiningJob_t;

/**
 * @brief SHA256 round constants
 * @note  Placed in DRAM (not flash) for faster access
 */
static PDQ_DRAM_ATTR const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    /* ... remaining 56 constants ... */
};
```

**Interface (Hot Path)**:

```c
/**
 * @brief Mine a range of nonces using SOFTWARE SHA256 (HOT PATH)
 * @param[in]     p_Job      Mining job with midstate, tail, target, nonce range
 * @param[out]    p_Nonce    Pointer to store found nonce
 * @param[out]    p_Found    Set to true if valid nonce found
 * @return PdqOk on success (check p_Found for result)
 * @note Compiled with -Os (GCC push_options)
 */
PdqError_t PdqSha256MineBlock(const PdqMiningJob_t *p_Job,
                               uint32_t *p_Nonce,
                               bool *p_Found);

/**
 * @brief Mine a range of nonces using HARDWARE SHA256 peripheral (HOT PATH)
 * @param[in]     p_Job      Mining job with midstate, tail, target, nonce range
 * @param[out]    p_Nonce    Pointer to store found nonce
 * @param[out]    p_Found    Set to true if valid nonce found
 * @return PdqOk on success (check p_Found for result)
 * @note Uses ESP32 SHA peripheral at SHA_TEXT_BASE (0x3FF03000)
 * @note Compiled with -O2 (GCC push_options)
 * @note Achieves ~650 KH/s (~369 cycles/nonce) with overlap + chaining optimization
 */
PdqError_t PdqSha256MineBlockHw(const PdqMiningJob_t *p_Job,
                                 uint32_t *p_Nonce,
                                 bool *p_Found);

/**
 * @brief Run boot-time hardware SHA diagnostic
 * @note Tests LOAD direction, overlap safety, auto-store, timing
 * @note Called once during setup() to validate HW SHA behavior
 */
void PdqSha256HwDiagnostic(void);
```

**Security Considerations for SHA256 Engine**:

| Risk | Mitigation |
|------|------------|
| Buffer overflow in Update | Validate Length against remaining buffer space |
| NULL pointer dereference | Check all pointers at function entry |
| Integer overflow in ByteCount | uint32_t sufficient for mining (max 80 bytes) |
| Cross-core data race | Each core receives own PdqMiningJob_t copy |

### 4.2 Mining Task

**Purpose**: Manages hash computation across multiple cores with proper ESP32 task management.

**Design**:

```
┌─────────────────────────────────────────────────────────────────┐
│                   Dual-Task Mining Architecture                  │
│                                                                  │
│  Core 0 (PRO_CPU)                    Core 1 (APP_CPU)           │
│  ┌──────────────────────┐           ┌──────────────────────┐   │
│  │ MiningTaskHw         │           │ MiningTaskCore1      │   │
│  │ Priority: 5          │           │ Priority: 5          │   │
│  │                      │           │                      │   │
│  │ Nonce: 0x20000000    │           │ Nonce: 0x00000000    │   │
│  │     to 0xFFFFFFFF    │           │     to 0x1FFFFFFF    │   │
│  │ (7/8 nonce space)    │           │ (1/8 nonce space)    │   │
│  │                      │           │                      │   │
│  │ [HW SHA256 Engine]   │           │ [SW SHA256 Engine]   │
│  │ PdqSha256MineBlockHw │           │ PdqSha256MineBlock   │   │
│  │ ~650 KH/s            │           │ ~46 KH/s             │   │
│  │ ~369 cyc/nonce        │           │                      │   │
│  │ Batch: 128K nonces   │           │ Batch: 4096 nonces   │   │
│  └──────────┬───────────┘           └──────────┬───────────┘   │
│             │                                   │                │
│             └───────────────┬───────────────────┘                │
│                             │                                    │
│                             ▼                                    │
│                    ┌────────────────┐                           │
│                    │ Share Queue    │                           │
│                    │ (FreeRTOS)     │                           │
│                    │ (xQueueSend)   │                           │
│                    └────────────────┘                           │
│                                                                  │
│  Combined: ~700 KH/s (HW ~650 + SW ~46)                        │
└─────────────────────────────────────────────────────────────────┘
```

> **Note**: Core 0 also runs WiFi/system tasks, but the HW SHA engine yields between 128K-nonce batches (~200ms each at ~650 KH/s), preventing IDLE task WDT starvation.

**Task Configuration**:

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Stack Size | 8192 bytes | Sufficient for SHA256 W[64] array on stack |
| Priority | 5 | High priority for mining, configurable |
| Core Affinity | Pinned (`xTaskCreatePinnedToCore`) | No migration overhead |
| WDT Feed | Time-based, every 1000ms (SW) / per-batch (HW) | Balance throughput/safety |
| SW Nonce Batch | 4096 | Per SW-core batch size |
| HW Nonce Batch | 131072 (128K) | Per HW batch size (~200ms at ~650 KH/s) |
| Nonce Split | SW 1/8 (0x00-0x1F), HW 7/8 (0x20-0xFF) | HW gets majority of nonce space |
| Stack Placement | DRAM (default) | Not IRAM to preserve IRAM for code |

**Watchdog Timer (WDT) Management**:

```c
/**
 * @brief HW Mining task for Core 0 (nonces 0x20000000-0xFFFFFFFF)
 * @note  Uses ESP32 SHA256 hardware peripheral for acceleration
 * @note  Core 1 runs MiningTaskCore1 (SW) with nonces 0x00000000-0x1FFFFFFF
 */
PDQ_IRAM_ATTR static void MiningTaskHw(void* p_Param)
{
    esp_task_wdt_add(NULL);
    uint32_t LocalHashes = 0;
    uint32_t LastHashReport = 0;

    while (s_State.Running) {
        if (!s_State.HasJob) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        xSemaphoreTake(s_State.JobMutex, portMAX_DELAY);
        PdqMiningJob_t Job = s_State.CurrentJob;
        uint32_t MyJobVersion = s_State.JobVersion;
        Job.NonceStart = PDQ_HW_NONCE_START;  /* 0x20000000 */
        Job.NonceEnd = PDQ_HW_NONCE_END;      /* 0xFFFFFFFF */
        xSemaphoreGive(s_State.JobMutex);

        uint32_t Base = Job.NonceStart;
        uint32_t JobEnd = Job.NonceEnd;
        while (s_State.Running && s_State.JobVersion == MyJobVersion) {
            Job.NonceStart = Base;
            uint32_t BatchEnd = Base + PDQ_NONCE_BATCH_SIZE_HW - 1;
            if (BatchEnd > JobEnd || BatchEnd < Base) BatchEnd = JobEnd;
            Job.NonceEnd = BatchEnd;

            uint32_t Nonce;
            bool Found;
            PdqSha256MineBlockHw(&Job, &Nonce, &Found);

            LocalHashes += (Job.NonceEnd - Job.NonceStart + 1);

            if (Found) {
                QueueShare(&Job, Nonce);
                __atomic_fetch_add(&s_State.BlocksFound, 1, __ATOMIC_RELAXED);
            }

            /* HW task must yield after every batch to prevent IDLE task WDT.
             * Each 128K batch takes ~200ms at ~650 KH/s. */
            esp_task_wdt_reset();
            vTaskDelay(1);

            uint32_t Now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (Now - LastHashReport > 1000) {
                __atomic_fetch_add(&s_State.TotalHashes, LocalHashes, __ATOMIC_RELAXED);
                LocalHashes = 0;
                LastHashReport = Now;
            }

            /* Overflow-safe loop termination */
            if (BatchEnd >= JobEnd) break;
            Base = BatchEnd + 1;
        }
    }

    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}
```

**Task Safety Checklist**:

| Requirement | Implementation |
|-------------|----------------|
| Stack overflow detection | `configCHECK_FOR_STACK_OVERFLOW = 2` in FreeRTOSConfig.h |
| Stack canaries | ESP-IDF default enabled |
| No heap allocation in hot path | All buffers pre-allocated at init |
| No floating point in mining loop | SHA256 is integer-only |
| Interrupt safety | Mining loop does not disable interrupts |
| Mutex-free share queue | FreeRTOS queue is ISR-safe |

### 4.3 Stratum Client

**Purpose**: Communicate with mining pool using Stratum protocol with secure input handling.

**Protocol Flow**:

```
Client                                      Server
  │                                            │
  │──── mining.subscribe ─────────────────────▶│
  │◀─── subscription result ──────────────────│
  │                                            │
  │──── mining.authorize ─────────────────────▶│
  │◀─── authorization result ─────────────────│
  │                                            │
  │◀─── mining.set_difficulty ────────────────│
  │◀─── mining.notify (new job) ──────────────│
  │                                            │
  │     [Mining in progress...]                │
  │                                            │
  │──── mining.submit (share) ────────────────▶│
  │◀─── submission result ────────────────────│
  │                                            │
```

**Data Structures with Size Constraints**:

```c
/** Maximum sizes for Stratum data (defined in stratum_client.h) */
#define PDQ_STRATUM_MAX_JOBID_LEN       64   /**< Job ID + null terminator */
#define PDQ_STRATUM_MAX_EXTRANONCE_LEN  8    /**< Max extranonce1 bytes */
#define PDQ_STRATUM_MAX_COINBASE_LEN    256  /**< Coinbase1/2 bytes */
#define PDQ_STRATUM_MAX_MERKLE_BRANCHES 16   /**< Max merkle branches */
#define PDQ_STRATUM_RECV_BUFFER_SIZE    4096 /**< TCP receive buffer */
#define PDQ_STRATUM_SEND_BUFFER_SIZE    512  /**< TCP send buffer */
#define PDQ_STRATUM_DEFAULT_TIMEOUT_MS  30000 /**< Connection timeout */

/** Size limits from pdq_types.h (used in config structs) */
#define PDQ_MAX_HOST_LEN                64   /**< Pool hostname */
#define PDQ_MAX_PASSWORD_LEN            64   /**< WiFi/pool password */
#define PDQ_MAX_WALLET_LEN              64   /**< BTC wallet address */
#define PDQ_MAX_WORKER_LEN              128  /**< Worker name */

/**
 * @brief Pool configuration (from pdq_types.h)
 */
typedef struct {
    char     Host[PDQ_MAX_HOST_LEN + 1];
    uint16_t Port;
    char     Password[PDQ_MAX_PASSWORD_LEN + 1];
} PdqPoolConfig_t;

/**
 * @brief Mining job from pool (Stratum notify data)
 * @note  Variable-length fields track actual length for safety
 */
typedef struct {
    char     JobId[PDQ_STRATUM_MAX_JOBID_LEN + 1];
    uint8_t  PrevBlockHash[32];
    uint8_t  Coinbase1[PDQ_STRATUM_MAX_COINBASE_LEN];
    uint16_t Coinbase1Len;                              /**< Actual length */
    uint8_t  Coinbase2[PDQ_STRATUM_MAX_COINBASE_LEN];
    uint16_t Coinbase2Len;                              /**< Actual length */
    uint8_t  MerkleBranches[PDQ_STRATUM_MAX_MERKLE_BRANCHES][32];
    uint8_t  MerkleBranchCount;                         /**< Actual count (0-16) */
    uint32_t Version;
    uint32_t NBits;
    uint32_t NTime;
    bool     CleanJobs;
} PdqStratumJob_t;
```

**Interface with Error Handling**:

```c
PdqError_t PdqStratumInit(void);
PdqError_t PdqStratumConnect(const char* p_Host, uint16_t Port);
PdqError_t PdqStratumDisconnect(void);
PdqError_t PdqStratumSubscribe(void);
PdqError_t PdqStratumAuthorize(const char* p_Worker, const char* p_Password);
PdqError_t PdqStratumSubmitShare(const char* p_JobId, uint32_t Extranonce2,
                                  uint32_t Nonce, uint32_t NTime);
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
                                           const uint8_t* p_Extranonce1,
                                           uint8_t Extranonce1Len,
                                           uint32_t Extranonce2,
                                           uint8_t Extranonce2Len,
                                           uint32_t Difficulty,
                                           PdqMiningJob_t* p_MiningJob);
```

**Input Validation Requirements**:

| Field | Validation |
|-------|------------|
| PoolHost | Non-empty, <= 64 chars, valid hostname chars only |
| PoolPort | 1-65535 inclusive |
| WalletAddress | Non-empty, <= 64 chars, alphanumeric only |
| WorkerName | <= 128 chars, printable ASCII only |
| JobId | <= 64 chars, hex or alphanumeric only |
| Coinbase1/2 | <= 256 bytes, valid hex encoding |
| MerkleBranchCount | 0-16 inclusive |
| JSON Response | <= 4096 bytes, valid UTF-8 |

**Safe String Handling**:

```c
/**
 * @brief Safe string copy with null termination guarantee
 * @param[out] p_Dest   Destination buffer
 * @param[in]  p_Src    Source string (may be NULL)
 * @param[in]  DestSize Size of destination buffer
 * @return Number of chars copied (excluding null), or -EINVAL on error
 * @note  Always null-terminates if DestSize > 0
 */
static inline int32_t PdqSafeStrCopy(char *p_Dest, const char *p_Src, size_t DestSize)
{
    if (p_Dest == NULL || DestSize == 0) {
        return -EINVAL;
    }
    if (p_Src == NULL) {
        p_Dest[0] = '\0';
        return 0;
    }
    size_t SrcLen = strnlen(p_Src, DestSize);
    size_t CopyLen = (SrcLen < DestSize) ? SrcLen : (DestSize - 1);
    memcpy(p_Dest, p_Src, CopyLen);
    p_Dest[CopyLen] = '\0';
    return (int32_t)CopyLen;
}
```

### 4.4 Display Driver

**Purpose**: Abstract display hardware for ILI9341/ST7789 TFT controllers with TFT_eSPI integration.

**Interface** (from `display_driver.h`):

```c
/**
 * @brief Display mode selection
 * @note  Build-time selection via -DPDQ_HEADLESS or runtime via PdqDisplayInit()
 */
typedef enum {
    PdqDisplayModeHeadless = 0,  /**< No display output */
    PdqDisplayModeMinimal,       /**< Text-only mining stats, 1 FPS */
    PdqDisplayModeStandard       /**< Basic graphics, 2 FPS */
} PdqDisplayMode_t;

PdqError_t PdqDisplayInit(PdqDisplayMode_t Mode);
PdqError_t PdqDisplayUpdate(const PdqMinerStats_t* p_Stats);
PdqError_t PdqDisplayShowMessage(const char* p_Line1, const char* p_Line2);
PdqError_t PdqDisplaySetBrightness(uint8_t Percent);
PdqError_t PdqDisplayOff(void);
```

**Implementation Notes**:
- Uses Bodmer's TFT_eSPI library for hardware abstraction
- Display type (ILI9341/ST7789) selected via build flags, not runtime
- All functions return `PdqOk` immediately when `PDQ_HEADLESS` is defined
- Mining stats auto-format hashrate as H/s, KH/s, or MH/s
- Color-coded temperature: white (<55°C), orange (55-70°C), red (>70°C)
- Rate-limited to 500ms minimum between updates

### 4.5 WiFi Manager & Configuration Portal

**Purpose**: Provide user-friendly WiFi provisioning and mining configuration via captive portal, similar to NerdMiner and NMMiner implementations.

**Third-Party Library**: PDQminer uses [IotWebConf](https://github.com/prampec/IotWebConf) (or alternatively [WiFiManager](https://github.com/tzapu/WiFiManager)) for the captive portal functionality. This is the same approach used by NerdMiner v2.

> **Design Note**: Both NerdMiner and NMMiner use a captive portal approach where the device creates a WiFi hotspot on first boot. Users connect to this hotspot and configure settings via a web browser. PDQminer follows this proven UX pattern.

#### 4.5.1 Configuration Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    First Boot / Reset Configuration                      │
│                                                                          │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────┐    ┌─────────┐ │
│  │  Device     │───▶│  AP Mode     │───▶│  User       │───▶│ Config  │ │
│  │  Powers On  │    │  "PDQminer"  │    │  Connects   │    │ Portal  │ │
│  └─────────────┘    │  192.168.4.1 │    │  to Hotspot │    │ (Web)   │ │
│                     └──────────────┘    └─────────────┘    └────┬────┘ │
│                                                                  │      │
│                                                                  ▼      │
│                     ┌──────────────────────────────────────────────┐   │
│                     │         Web Configuration Form               │   │
│                     │  ┌────────────────────────────────────────┐  │   │
│                     │  │ WiFi SSID:     [________________]      │  │   │
│                     │  │ WiFi Password: [________________]      │  │   │
│                     │  │                                        │  │   │
│                     │  │ === Primary Pool ===                   │  │   │
│                     │  │ Pool URL:      [public-pool.io____]    │  │   │
│                     │  │ Pool Port:     [21496______________]   │  │   │
│                     │  │                                        │  │   │
│                     │  │ === Backup Pool ===                    │  │   │
│                     │  │ Pool URL:      [solo.ckpool.org___]    │  │   │
│                     │  │ Pool Port:     [3333_______________]   │  │   │
│                     │  │                                        │  │   │
│                     │  │ === Wallet ===                         │  │   │
│                     │  │ BTC Address:   [bc1q..._____________]  │  │   │
│                     │  │ Worker Name:   [pdqminer1__________]   │  │   │
│                     │  │                                        │  │   │
│                     │  │         [Save & Reboot]                │  │   │
│                     │  └────────────────────────────────────────┘  │   │
│                     └──────────────────────────────────────────────┘   │
│                                                                  │      │
│                                                                  ▼      │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────────────────┐│
│  │  Mining     │◀───│  Connect to  │◀───│  Save to NVS               ││
│  │  Starts     │    │  WiFi + Pool │    │  (Encrypted credentials)   ││
│  └─────────────┘    └──────────────┘    └─────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────┘
```

#### 4.5.2 Access Point Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| SSID | `PDQminer_XXXX` | XXXX = last 4 hex of MAC |
| Password | None (open) | Or configurable default |
| IP Address | `192.168.4.1` | Standard ESP32 AP IP |
| Captive Portal | Auto-redirect | Triggers on any HTTP request |
| Timeout | 5 minutes | Returns to mining if unconfigured |

#### 4.5.3 Configuration Parameters

**Data Structures** (from `pdq_types.h`):

```c
/**
 * @brief WiFi credentials
 */
typedef struct {
    char     Ssid[PDQ_MAX_SSID_LEN + 1];         /**< WiFi network name (max 32 chars) */
    char     Password[PDQ_MAX_PASSWORD_LEN + 1];  /**< WiFi password */
} PdqWifiConfig_t;

/**
 * @brief Mining pool configuration
 */
typedef struct {
    char     Host[PDQ_MAX_HOST_LEN + 1];          /**< Pool hostname (max 64 chars) */
    uint16_t Port;                                /**< Pool port (1-65535) */
    char     Password[PDQ_MAX_PASSWORD_LEN + 1];  /**< Pool password (optional) */
} PdqPoolConfig_t;

/**
 * @brief Complete device configuration (stored in NVS)
 */
typedef struct {
    PdqWifiConfig_t Wifi;                          /**< WiFi credentials */
    PdqPoolConfig_t PrimaryPool;                   /**< Primary mining pool */
    PdqPoolConfig_t BackupPool;                    /**< Backup pool (failover) */
    char            WalletAddress[PDQ_MAX_WALLET_LEN + 1]; /**< BTC address */
    char            WorkerName[PDQ_MAX_WORKER_LEN + 1];    /**< Worker identifier */
    uint8_t         DisplayMode;                   /**< 0=Headless, 1=Minimal, 2=Standard */
} PdqDeviceConfig_t;

/* Size constraints (from pdq_types.h) */
#define PDQ_MAX_SSID_LEN       32
#define PDQ_MAX_PASSWORD_LEN   64
#define PDQ_MAX_HOST_LEN       64
#define PDQ_MAX_WALLET_LEN     64
#define PDQ_MAX_WORKER_LEN     128
```

#### 4.5.4 Web Interface

The configuration portal serves a lightweight HTML page with the following features:

**Capabilities**:
- Responsive design (works on mobile)
- Real-time WiFi network scan
- Form validation (client-side and server-side)
- Status display (current hashrate, connection status)
- Firmware version display
- Factory reset option

**Endpoints**:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main configuration page |
| `/save` | POST | Save configuration, trigger reboot |
| `/scan` | GET | Return JSON list of available WiFi networks |
| `/status` | GET | Return JSON with current mining status |
| `/reset` | POST | Factory reset (clear NVS) |
| `/update` | POST | OTA firmware update (optional) |

**HTML Template** (embedded in firmware, gzip compressed):

```html
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>PDQminer Setup</title>
    <style>
        body { font-family: sans-serif; max-width: 400px; margin: 0 auto; padding: 20px; }
        input, select { width: 100%; padding: 8px; margin: 5px 0 15px 0; box-sizing: border-box; }
        button { background: #007bff; color: white; padding: 10px 20px; border: none; cursor: pointer; }
        .section { border: 1px solid #ddd; padding: 15px; margin: 10px 0; border-radius: 5px; }
        h3 { margin-top: 0; }
    </style>
</head>
<body>
    <h1>PDQminer Setup</h1>
    <form action="/save" method="POST">
        <div class="section">
            <h3>WiFi Network</h3>
            <label>SSID</label>
            <select name="ssid" id="ssid"></select>
            <label>Password</label>
            <input type="password" name="wifi_pass" maxlength="64">
        </div>
        <div class="section">
            <h3>Primary Pool</h3>
            <label>Host</label>
            <input type="text" name="pool1_host" value="public-pool.io" maxlength="63">
            <label>Port</label>
            <input type="number" name="pool1_port" value="21496" min="1" max="65535">
        </div>
        <div class="section">
            <h3>Backup Pool</h3>
            <label>Host</label>
            <input type="text" name="pool2_host" value="solo.ckpool.org" maxlength="63">
            <label>Port</label>
            <input type="number" name="pool2_port" value="3333" min="1" max="65535">
        </div>
        <div class="section">
            <h3>Wallet</h3>
            <label>BTC Address</label>
            <input type="text" name="wallet" maxlength="62" required
                   pattern="^(bc1|[13])[a-zA-HJ-NP-Z0-9]{25,62}$">
            <label>Worker Name</label>
            <input type="text" name="worker" value="pdqminer" maxlength="31">
        </div>
        <button type="submit">Save & Reboot</button>
    </form>
    <script>
        fetch('/scan').then(r=>r.json()).then(networks=>{
            const sel=document.getElementById('ssid');
            networks.forEach(n=>sel.add(new Option(n.ssid+' ('+n.rssi+'dBm)',n.ssid)));
        });
    </script>
</body>
</html>
```

#### 4.5.5 Configuration API

```c
/**
 * @brief WiFi Manager API (from wifi_manager.h)
 */
PdqError_t PdqWifiInit(void);
PdqError_t PdqWifiConnect(const char* p_Ssid, const char* p_Password);
PdqError_t PdqWifiDisconnect(void);
PdqError_t PdqWifiStartAp(const char* p_Ssid, const char* p_Password);
PdqError_t PdqWifiStopAp(void);
PdqError_t PdqWifiStartPortal(void);
PdqError_t PdqWifiStopPortal(void);
PdqError_t PdqWifiProcess(void);
PdqError_t PdqWifiScan(PdqWifiScanResult_t* p_Results, uint8_t* p_Count);

bool            PdqWifiIsConnected(void);
bool            PdqWifiIsPortalActive(void);
PdqWifiState_t  PdqWifiGetState(void);
PdqError_t      PdqWifiGetIp(char* p_Buffer, size_t BufferLen);
int8_t          PdqWifiGetRssi(void);

/**
 * @brief Config Manager API (from config_manager.h)
 */
PdqError_t PdqConfigInit(void);
PdqError_t PdqConfigLoad(PdqDeviceConfig_t* p_Config);
PdqError_t PdqConfigSave(const PdqDeviceConfig_t* p_Config);
PdqError_t PdqConfigReset(void);
bool       PdqConfigIsValid(void);
PdqError_t PdqConfigGetString(const char* p_Key, char* p_Value, size_t MaxLen);
PdqError_t PdqConfigSetString(const char* p_Key, const char* p_Value);
PdqError_t PdqConfigGetU16(const char* p_Key, uint16_t* p_Value);
PdqError_t PdqConfigSetU16(const char* p_Key, uint16_t Value);
```

#### 4.5.6 Pool Failover Logic

```c
/**
 * @brief Pool connection state machine
 *
 * Primary Pool ──[fail]──▶ Backup Pool ──[fail]──▶ Primary Pool (retry)
 *       ▲                        │
 *       │                        │
 *       └────[reconnect]─────────┘
 */
typedef enum {
    PoolStatePrimaryConnected,
    PoolStatePrimaryFailed,
    PoolStateBackupConnected,
    PoolStateBackupFailed,
    PoolStateReconnecting
} PoolState_t;

/* Failover timing */
#define PDQ_POOL_CONNECT_TIMEOUT_MS     10000   /* 10 sec connect timeout */
#define PDQ_POOL_RETRY_DELAY_MS         5000    /* 5 sec between retries */
#define PDQ_POOL_MAX_CONSECUTIVE_FAIL   3       /* Switch to backup after 3 fails */
```

#### 4.5.7 Security Considerations

| Aspect | Implementation |
|--------|----------------|
| WiFi password storage | NVS encrypted (CONFIG_NVS_ENCRYPTION) |
| Portal access | Open AP (local network only) |
| HTTPS for portal | Optional (self-signed cert) |
| Input sanitization | All form inputs validated server-side |
| CSRF protection | Not required (local AP only) |

#### 4.5.8 Triggering Configuration Mode

Users can enter configuration mode via:

| Method | Action |
|--------|--------|
| First boot | Automatic if no valid config |
| Button hold | Hold BOOT button for 5 seconds |
| Triple reset | Power cycle 3 times within 5 seconds |
| Serial command | Send `RESET_CONFIG` via USB serial |
| Display menu | Long-press touch (if display equipped) |
```

---

## 5. Data Flow

### 5.1 Mining Data Flow

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Pool    │     │ Stratum  │     │   Job    │     │ Mining   │
│  Server  │────▶│  Client  │────▶│ Manager  │────▶│  Tasks   │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
     ▲                                                   │
     │                                                   │
     │              ┌──────────┐                         │
     └──────────────│  Share   │◀────────────────────────┘
                    │  Queue   │
                    └──────────┘
```

### 5.2 Job Processing

1. **Stratum Client** receives `mining.notify` with new job
2. **Job Manager** constructs 80-byte block header
3. **Job Manager** computes midstate for first 64 bytes
4. **Job Manager** distributes job to mining tasks
5. **Mining Tasks** iterate nonces, checking target
6. **Mining Tasks** push valid shares to share queue
7. **Stratum Client** submits shares to pool

---

## 6. Performance Considerations

### 6.1 Hashrate Optimization Progress

| Optimization | Expected Gain | Actual Result | Status |
|--------------|---------------|---------------|--------|
| CPU @ 240MHz | Baseline | Baseline | Complete |
| Midstate caching | +50% | ~58 KH/s SW | Complete |
| Nonce-only update | +100% | Included in SW | Complete |
| Full round unrolling | +30% | Included in SW | Complete |
| Dual-core (SW+SW) | +95% | ~58 KH/s (2 cores) | Complete |
| **HW SHA256 peripheral** | **+700%** | **472 KH/s** | **Complete** |
| **Overlap register writes** | **+31%** | **627 KH/s** | **Complete** |
| **START→CONTINUE chaining** | **+12%** | **~700 KH/s** | **Complete** |
| Midstate on HW | N/A | **Impossible on ESP32-D0** | Investigated |
| IRAM for HW path | +5% est | **Regressed (538 vs 413 cyc)** | Rejected |

**Current Hashrate**: ~700 KH/s (HW ~650 KH/s + SW ~46 KH/s)
**Target**: ≥1000 KH/s

### 6.2 Memory Budget

| Component | RAM Usage | Notes |
|-----------|-----------|-------|
| Mining Context (x2) | 512 bytes | Per-core |
| Job Queue | 2 KB | Circular buffer |
| Stratum Buffer | 4 KB | JSON parsing |
| Display Buffer | 0 KB | Direct SPI write |
| WiFi Stack | 32 KB | ESP-IDF managed |
| FreeRTOS | 16 KB | Tasks + queues |
| **Total** | ~55 KB | Of 320 KB available |

### 6.3 Display Optimization Strategy

Display operations consume CPU cycles via SPI bus transactions. PDQminer offers three display modes to maximize hashrate.

#### 6.3.1 Display Modes

| Mode | Description | KH/s Impact | Use Case |
|------|-------------|-------------|----------|
| **Headless** | No display driver loaded | +20-50 KH/s | ESP32 boards without display, mining farms |
| **Minimal** | Text-only, 1 FPS | +10-20 KH/s | Performance-focused with basic monitoring |
| **Standard** | Simple graphics, 2 FPS | Baseline | Balance of info and performance |

> **Design Decision**: PDQminer prioritizes hashrate over fancy graphics. Complex animations and high-FPS updates are explicitly avoided.

#### 6.3.2 Minimal Display Mode (Default for CYD)

The minimal display shows only essential mining information:

```
┌────────────────────────────────────────┐
│           PDQminer v1.0.0              │
│                                        │
│   Hashrate:     1,024.5 KH/s           │
│   Best Diff:    2.5M                   │
│   Shares:       142 / 3 rejected       │
│   Blocks:       0                      │
│                                        │
│   Pool:         solo.ckpool.org        │
│   Uptime:       4h 23m                 │
│                                        │
│   BTC:          $98,432                │
│   WiFi:         ████████░░  -52 dBm    │
└────────────────────────────────────────┘
```

**Minimal Mode Specifications**:
- **Font**: Monospace, single size (no scaling overhead)
- **Colors**: 2-3 colors maximum (foreground, background, highlight)
- **Update Rate**: 1 FPS (1000ms interval)
- **Update Method**: Text-only dirty regions (no full redraws)
- **Graphics**: None (no icons, animations, or images)
- **BTC Price**: Updated every 5 minutes via CoinGecko API (optional)

#### 6.3.3 Headless Mode

For ESP32 boards without displays or maximum performance scenarios:

- **Build Flag**: `-DDISPLAY_MODE=HEADLESS`
- **Behavior**: Display driver not compiled; GPIO pins freed
- **Monitoring**: Serial output only (115200 baud)
- **Status LED**: Optional heartbeat on GPIO (configurable)

**Serial Output Format** (headless mode):
```
[PDQ] Hashrate: 1052.3 KH/s | Shares: 142/3 | Uptime: 4h23m
[PDQ] Hashrate: 1048.7 KH/s | Shares: 143/3 | Uptime: 4h24m
```

#### 6.3.4 Display Task Configuration

| Parameter | Headless | Minimal | Standard |
|-----------|----------|---------|----------|
| Task Created | No | Yes | Yes |
| Stack Size | N/A | 2048 bytes | 4096 bytes |
| Priority | N/A | `tskIDLE_PRIORITY + 1` | `tskIDLE_PRIORITY + 1` |
| Core Affinity | N/A | Core 0 | Core 0 |
| Update Interval | N/A | 1000 ms | 500 ms |
| SPI Transactions/Update | 0 | ~50 | ~200 |

#### 6.3.5 Implementation Notes

```c
/* Display mode compile-time selection */
#if DISPLAY_MODE == HEADLESS
    /* No display task created */
    #define PdqDisplayInit(cfg)     (0)
    #define PdqDisplayUpdate()      (0)
#elif DISPLAY_MODE == MINIMAL
    /* Minimal text-only display */
    int32_t PdqDisplayInit(const DisplayConfig_t *p_Config);
    int32_t PdqDisplayUpdateMinimal(const MiningStats_t *p_Stats);
#else
    /* Standard display with basic graphics */
    int32_t PdqDisplayInit(const DisplayConfig_t *p_Config);
    int32_t PdqDisplayUpdate(const MiningStats_t *p_Stats);
#endif
```

---

## 7. Board Support

### 7.1 ESP32-2432S028R Configuration

| Parameter | ILI9341 Variant | ST7789 Variant |
|-----------|-----------------|----------------|
| Display Width | 320 | 320 |
| Display Height | 240 | 240 |
| SPI Clock | 40 MHz | 40 MHz |
| MOSI Pin | GPIO 13 | GPIO 13 |
| SCLK Pin | GPIO 14 | GPIO 14 |
| CS Pin | GPIO 15 | GPIO 15 |
| DC Pin | GPIO 2 | GPIO 2 |
| RST Pin | GPIO 12 | GPIO 12 (or -1) |
| BL Pin | GPIO 21 | GPIO 21 |
| Touch Type | Resistive XPT2046 | Resistive XPT2046 |

### 7.2 Build Targets

```ini
; PDQminer uses Arduino framework (not ESP-IDF directly)
; Full configuration in platformio.ini at project root

[env]
platform = espressif32@6.5.0
framework = arduino
board_build.f_cpu = 240000000L
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DPDQ_VERSION=\"0.1.0\"
    -O3
    -funroll-loops
    -ffast-math
    -fomit-frame-pointer
    -finline-functions
    -finline-limit=10000
    -fno-strict-aliasing
    -ftree-vectorize
    -ffunction-sections
    -fdata-sections
    -mtext-section-literals

; =============================================================================
; CYD Boards with Display
; =============================================================================

[env:cyd_ili9341]
board = esp32dev
build_flags =
    ${env.build_flags}
    -DPDQ_BOARD_CYD
    -DPDQ_DISPLAY_ILI9341
    -DUSER_SETUP_LOADED
    -DILI9341_DRIVER
    ; ... TFT_eSPI pin definitions ...
lib_deps = bodmer/TFT_eSPI@^2.5.43

[env:cyd_st7789]
board = esp32dev
build_flags =
    ${env.build_flags}
    -DPDQ_BOARD_CYD
    -DPDQ_DISPLAY_ST7789
    -DUSER_SETUP_LOADED
    -DST7789_DRIVER
    ; ... TFT_eSPI pin definitions ...
lib_deps = bodmer/TFT_eSPI@^2.5.43

; =============================================================================
; Headless Builds (Maximum Hashrate)
; =============================================================================

[env:cyd_ili9341_headless]
board = esp32dev
build_flags =
    ${env.build_flags}
    -DPDQ_BOARD_CYD
    -DPDQ_HEADLESS

[env:esp32_headless]
board = esp32dev
build_flags =
    ${env.build_flags}
    -DPDQ_HEADLESS

; =============================================================================
; Development, Debug & Benchmark
; =============================================================================

[env:cyd_debug]
board = esp32dev
build_type = debug
build_flags = ... -DPDQ_DEBUG -Og

[env:benchmark]
board = esp32dev
build_flags = ... -DPDQ_HEADLESS -DPDQ_BENCHMARK
build_src_filter = +<*> -<main.cpp> +<main_benchmark.cpp>

[env:native]
platform = native
test_framework = unity

; =============================================================================
; ESP8266 Support (single-core, reduced performance)
; =============================================================================

[env:esp8266_headless]
platform = espressif8266@4.2.1
board = nodemcuv2
board_build.f_cpu = 160000000L
build_flags =
    -DPDQ_HEADLESS
    -DESP8266
    -O3
```

### 7.3 Expected Hashrate by Build

| Build Target | Display Mode | Platform | Measured Hashrate |
|--------------|--------------|----------|-------------------|
| `cyd_ili9341` | Minimal | ESP32 | **~700 KH/s** |
| `cyd_st7789` | Minimal | ESP32 | **~700 KH/s** |
| `esp32_headless` | Headless | ESP32 | ~630+ KH/s (est.) |
| `cyd_ili9341_headless` | Headless | ESP32 | ~630+ KH/s (est.) |
| `benchmark` | Headless | ESP32 | Raw measurement |
| `esp8266_headless` | Headless | ESP8266 | TBD (single-core, no HW SHA) |

> **Note**: Headless builds are expected to gain 20-50 KH/s over display-enabled builds due to eliminated SPI overhead and freed CPU cycles.
> **Note**: ESP8266 is single-core at 160MHz. Hashrate will be significantly lower than ESP32.

---

## 8. Security Considerations

This section documents security requirements and defensive coding practices for PDQminer.

### 8.1 Credential Storage

| Data Type | Storage Method | Security Level |
|-----------|---------------|----------------|
| WiFi SSID | NVS with encryption | Encrypted at rest |
| WiFi Password | NVS with encryption | Encrypted at rest |
| BTC Wallet Address | NVS plain text | Public by design |
| Pool Password | NVS with encryption | Encrypted at rest |
| Private Keys | **NOT STORED** | N/A (never on device) |

**NVS Encryption Configuration**:
```c
/* Enable NVS encryption in sdkconfig */
CONFIG_NVS_ENCRYPTION=y
CONFIG_SECURE_FLASH_ENC_ENABLED=y  /* Optional: full flash encryption */
```

### 8.2 Network Security

| Requirement | Implementation |
|-------------|----------------|
| TLS for pool connections | Optional; mbedTLS via ESP-IDF |
| Certificate validation | Enabled when TLS is used |
| Input validation | All Stratum JSON responses validated |
| Buffer overflow protection | Fixed-size buffers with length tracking |
| Timeout handling | All network ops have configurable timeouts |

### 8.3 Memory Safety

**Buffer Overflow Prevention**:

```c
/* ALWAYS use size-bounded operations */

/* BAD - vulnerable to overflow */
strcpy(dest, src);
sprintf(buf, "Hello %s", name);

/* GOOD - bounded and safe */
PdqSafeStrCopy(dest, src, sizeof(dest));
snprintf(buf, sizeof(buf), "Hello %s", name);

/* GOOD - explicit length validation */
if (len > sizeof(buffer)) {
    return -EINVAL;
}
memcpy(buffer, data, len);
```

**Integer Overflow Prevention**:

```c
/* BAD - potential overflow */
uint32_t total = count * size;  /* May wrap on large values */

/* GOOD - check before multiply */
if (count > SIZE_MAX / size) {
    return -EOVERFLOW;
}
uint32_t total = count * size;

/* GOOD - use safe math functions */
size_t total;
if (__builtin_mul_overflow(count, size, &total)) {
    return -EOVERFLOW;
}
```

**Null Pointer Validation**:

```c
/* Every public function validates pointers at entry */
int32_t PdqFunctionExample(SomeStruct_t *p_Struct, const uint8_t *p_Data)
{
    /* Validate required pointers immediately */
    if (p_Struct == NULL) {
        return -EINVAL;
    }
    if (p_Data == NULL && RequiresData) {
        return -EINVAL;
    }

    /* Proceed with validated pointers */
    ...
}
```

### 8.4 ESP32-Specific Security

**Task Stack Overflow Detection**:

```c
/* FreeRTOSConfig.h settings */
#define configCHECK_FOR_STACK_OVERFLOW  2  /* Full checking */
#define configUSE_MALLOC_FAILED_HOOK    1  /* Catch alloc failures */

/* Stack overflow hook */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    ESP_LOGE("STACK", "Overflow in task: %s", pcTaskName);
    esp_restart();  /* Recovery via restart */
}
```

**Heap Canaries** (ESP-IDF default):
- Enabled via `CONFIG_HEAP_POISONING_COMPREHENSIVE=y`
- Detects heap corruption on free()

**Watchdog Timer Configuration**:

```c
/* Task WDT settings */
#define CONFIG_ESP_TASK_WDT_TIMEOUT_S  5    /* 5 second timeout */
#define CONFIG_ESP_TASK_WDT_PANIC      y    /* Panic on WDT timeout */

/* Mining tasks must feed WDT regularly */
esp_task_wdt_add(NULL);   /* Register task */
esp_task_wdt_reset();     /* Feed WDT in loop */
```

### 8.5 Input Validation Rules

All external input (network, serial, user) MUST be validated:

| Input Source | Validation Required |
|-------------|---------------------|
| JSON from pool | Schema validation, length limits |
| WiFi credentials | Length limits, character set validation |
| Hostname/IP | Valid hostname format or IP address |
| Port numbers | Range: 1-65535 |
| Hex strings | Even length, valid hex chars only |
| Block headers | Exactly 80 bytes |
| Nonce values | Any uint32_t valid (full range) |

**Hex String Validation Example**:

```c
/**
 * @brief Validate hex string and convert to bytes
 * @param[in]  p_Hex    Hex string (must not be NULL)
 * @param[out] p_Bytes  Output buffer (must not be NULL)
 * @param[in]  MaxBytes Maximum bytes to output
 * @return Number of bytes written, or negative error
 */
int32_t PdqHexToBytes(const char *p_Hex, uint8_t *p_Bytes, size_t MaxBytes)
{
    if (p_Hex == NULL || p_Bytes == NULL || MaxBytes == 0) {
        return -EINVAL;
    }

    size_t HexLen = strnlen(p_Hex, MaxBytes * 2 + 1);
    if (HexLen % 2 != 0) {
        return -EINVAL;  /* Odd length */
    }

    size_t ByteCount = HexLen / 2;
    if (ByteCount > MaxBytes) {
        return -ENOBUFS;  /* Output too small */
    }

    for (size_t i = 0; i < ByteCount; i++) {
        char High = p_Hex[i * 2];
        char Low = p_Hex[i * 2 + 1];

        if (!isxdigit(High) || !isxdigit(Low)) {
            return -EINVAL;  /* Invalid hex char */
        }

        p_Bytes[i] = (HexCharToNibble(High) << 4) | HexCharToNibble(Low);
    }

    return (int32_t)ByteCount;
}
```

### 8.6 Error Handling Policy

**Error Return Convention**:
- Success: Return 0 or positive value (count, size, etc.)
- Failure: Return negative errno-style value

**Common Error Codes**:

| Code | Meaning | Usage |
|------|---------|-------|
| `-EINVAL` | Invalid argument | NULL pointer, bad parameter |
| `-ENOMEM` | Out of memory | Allocation failed |
| `-ENOBUFS` | Buffer too small | Output buffer insufficient |
| `-ETIMEDOUT` | Timeout | Network or wait timeout |
| `-ECONNREFUSED` | Connection refused | Pool connection failed |
| `-EOVERFLOW` | Overflow | Integer overflow detected |

**Error Logging**:

```c
/* Log errors with context for debugging */
if (Result < 0) {
    ESP_LOGE(TAG, "%s failed: %d (%s)", __func__, Result, strerror(-Result));
    return Result;
}
```

### 8.7 Secure Coding Checklist

Before merging any code, verify:

- [ ] All pointers validated before dereference
- [ ] All buffers have explicit size limits
- [ ] No `strcpy`, `sprintf`, `gets` used (use safe variants)
- [ ] Integer operations checked for overflow where applicable
- [ ] Network input length-limited before parsing
- [ ] Error paths clean up allocated resources
- [ ] No sensitive data in log messages
- [ ] WDT feeding in long-running loops
- [ ] Stack usage estimated and within limits
- [ ] Sensitive memory zeroed after use

### 8.8 Side-Channel Attack Resistance

**Constant-Time SHA256 Implementation**:

SHA256 operations MUST be constant-time to prevent timing attacks:

```c
/* BAD - variable-time comparison */
if (memcmp(hash, target, 32) <= 0) {
    return true;  /* Timing leak on early mismatch */
}

/* GOOD - constant-time comparison */
IRAM_ATTR bool PdqConstantTimeCompare(const uint32_t *p_Hash,
                                       const uint32_t *p_Target)
{
    uint32_t Diff = 0;
    for (int i = 0; i < 8; i++) {
        /* Compare all words, accumulate difference */
        Diff |= (p_Hash[i] ^ p_Target[i]);
    }
    /* Additional timing-safe less-than check for mining target */
    /* ... implementation ... */
    return (Diff == 0);
}
```

**Power Analysis Mitigation**:
- ESP32 lacks hardware countermeasures
- Software mitigation: avoid key-dependent branching (N/A for mining)
- Mining does not process secrets, so power analysis is low risk

### 8.9 DoS Protection

**Stratum Protocol Hardening**:

| Attack Vector | Mitigation |
|--------------|------------|
| Oversized JSON | Max 4KB per message; reject larger |
| Rapid reconnect | Exponential backoff (1s, 2s, 4s... max 60s) |
| Malformed jobs | Validate all fields; discard invalid jobs |
| Memory exhaustion | Fixed-size buffers; no dynamic allocation in parser |
| CPU exhaustion | Already mining at 100%; no additional load possible |

**Connection Limits**:
```c
#define PDQ_STRATUM_MAX_MESSAGE_SIZE    4096
#define PDQ_STRATUM_RECONNECT_MIN_MS    1000
#define PDQ_STRATUM_RECONNECT_MAX_MS    60000
#define PDQ_STRATUM_READ_TIMEOUT_MS     30000
```

### 8.10 Secure Boot & Firmware Signing (Optional)

**ESP32 Secure Boot Configuration**:

For production deployments requiring anti-tampering:

```c
/* sdkconfig options for secure boot */
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_key.pem"
CONFIG_SECURE_FLASH_ENC_ENABLED=y
```

**Firmware Signing Workflow**:
1. Generate signing key: `espsecure.py generate_signing_key`
2. Build with signing enabled
3. Signed binaries verified by bootloader before execution
4. Prevents unauthorized firmware modification

> **Note**: Secure boot is OPTIONAL. Most solo miners don't require it, but it's documented for enterprise/farm deployments.

### 8.11 Memory Sanitization

**Sensitive Data Handling**:

```c
/* Zero sensitive buffers after use */
void PdqSecureZero(void *p_Buffer, size_t Size)
{
    volatile uint8_t *p = (volatile uint8_t *)p_Buffer;
    while (Size--) {
        *p++ = 0;
    }
    /* Compiler cannot optimize away volatile writes */
}

/* Example: Clear WiFi password after use */
char WifiPassword[64];
/* ... use password ... */
PdqSecureZero(WifiPassword, sizeof(WifiPassword));
```

**Memory Protection**:
- Stack canaries enabled (ESP-IDF default)
- Heap poisoning enabled for debug builds
- No sensitive data persisted in RAM longer than necessary

### 8.12 Supply Chain Security

**Dependency Management**:
- Pin all ESP-IDF and library versions in `platformio.ini`
- Verify library hashes in CI pipeline
- No third-party binaries; all code compiled from source

**Build Reproducibility**:
- Documented toolchain versions
- Deterministic builds when possible
- CI builds produce identical binaries

### 8.13 Security Audit Summary

| Category | Status | Notes |
|----------|--------|-------|
| Buffer overflow | Protected | All buffers size-bounded |
| Integer overflow | Protected | Checked before operations |
| NULL pointers | Protected | Validated at function entry |
| Memory leaks | Protected | No dynamic allocation in hot path |
| Use-after-free | Protected | Minimal heap usage; clear ownership |
| Stack overflow | Protected | FreeRTOS canaries enabled |
| Timing attacks | Protected | Constant-time SHA256 |
| DoS (network) | Protected | Message size limits, timeouts |
| DoS (local) | Protected | WDT prevents infinite loops |
| Credential storage | Protected | NVS encryption for passwords |
| Code injection | Protected | No dynamic code execution |
| Secure boot | Optional | Available for high-security deployments |

---

## 9. Future Considerations

### 9.1 Potential Enhancements

- Further HW SHA optimization (reduce 413 cyc/nonce overhead)
- Assembly-optimized SHA256 rounds (Xtensa-specific)
- Multi-algorithm support (Scrypt, etc.)
- ASIC chip support (BM1397)
- Daisy-chain multiple devices
- ESP32-S2/S3/C3 midstate caching (these chips have `SHA_H_BASE` register)

### 9.2 Scalability

Architecture supports:
- Additional board variants via HAL
- Multiple display drivers
- Alternative pool protocols

---

## 10. Firmware Patcher Tools

PDQminer includes cross-platform tools for flashing firmware to ESP32 devices, making it accessible to users without development environments.

### 10.1 Overview

| Tool | Platform | Use Case |
|------|----------|----------|
| Web Flasher | Browser (Chrome/Edge) | Quick one-click flashing via WebSerial |
| Python Flasher | Windows/macOS/Linux | CLI flashing, scripting, CI/CD |

### 10.2 Web Flasher (`tools/web-flasher/`)

Browser-based flashing using [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

#### 10.2.1 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Web Browser                             │
│  ┌───────────────┐  ┌───────────────┐  ┌────────────────┐  │
│  │  index.html   │──│  flasher.js   │──│ WebSerial API  │  │
│  └───────────────┘  └───────────────┘  └───────┬────────┘  │
└───────────────────────────────────────────────────┼─────────┘
                                                    │ USB
┌───────────────────────────────────────────────────▼─────────┐
│                    ESP32 Device                              │
│                    (Bootloader Mode)                         │
└─────────────────────────────────────────────────────────────┘
```

#### 10.2.2 Features

- **One-click install**: No software installation required
- **Board selection**: Dropdown for supported boards (ILI9341/ST7789)
- **Progress feedback**: Real-time flash progress
- **Error handling**: Clear error messages for common issues
- **Hosted on GitHub Pages**: `https://pdqminer.github.io/flash`

#### 10.2.3 Manifest Structure

```json
{
  "name": "PDQminer",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "pdqminer_cyd_ili9341.bin", "offset": 0 }
      ]
    }
  ]
}
```

#### 10.2.4 File Structure

```
tools/web-flasher/
├── index.html              # Main page with ESP Web Tools
├── manifest.json           # Firmware manifest
├── css/
│   └── style.css           # PDQminer branding
├── js/
│   └── flasher.js          # Board selection logic
└── firmware/               # Binary files (populated by CI)
    ├── pdqminer_cyd_ili9341.bin
    └── pdqminer_cyd_st7789.bin
```

### 10.3 Python Flasher (`tools/python-flasher/`)

Cross-platform CLI tool built on `esptool.py`.

#### 10.3.1 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    pdqflash CLI                              │
│  ┌───────────────┐  ┌───────────────┐  ┌────────────────┐  │
│  │  pdqflash.py  │──│  flasher.py   │──│  esptool.py    │  │
│  │  (argparse)   │  │  (logic)      │  │  (low-level)   │  │
│  └───────────────┘  └───────────────┘  └───────┬────────┘  │
└───────────────────────────────────────────────────┼─────────┘
                                                    │ Serial
┌───────────────────────────────────────────────────▼─────────┐
│                    ESP32 Device                              │
└─────────────────────────────────────────────────────────────┘
```

#### 10.3.2 Features

- **Auto-detect board**: Identify connected ESP32 variant
- **Auto-detect port**: Find serial port automatically
- **Multiple boards**: Flash configuration per board type
- **Verify after flash**: Optional checksum verification
- **Erase option**: Full flash erase before writing
- **Progress bar**: Visual feedback during flash

#### 10.3.3 CLI Interface

```bash
# Basic usage - auto-detect board and port
pdqflash flash

# Specify board explicitly
pdqflash flash --board cyd_ili9341

# Specify port
pdqflash flash --port /dev/ttyUSB0

# Flash specific binary
pdqflash flash --binary ./custom_firmware.bin

# List detected boards
pdqflash detect

# Erase flash completely
pdqflash erase --port /dev/ttyUSB0

# Show version info
pdqflash --version
```

#### 10.3.4 File Structure

```
tools/python-flasher/
├── pdqflash.py             # CLI entry point
├── requirements.txt        # Dependencies
├── setup.py                # Package setup (pip install)
├── pdqflash/               # Package module
│   ├── __init__.py
│   ├── flasher.py          # Flash operations
│   ├── detector.py         # Board/port detection
│   └── config.py           # Board configurations
└── tests/
    ├── test_flasher.py
    └── test_detector.py
```

#### 10.3.5 Dependencies

```
# requirements.txt
esptool>=4.0
pyserial>=3.5
click>=8.0        # CLI framework
rich>=13.0        # Progress bars and formatting
```

#### 10.3.6 Board Configuration

```python
# pdqflash/config.py
BOARD_CONFIGS = {
    "cyd_ili9341": {
        "chip": "esp32",
        "flash_size": "4MB",
        "flash_mode": "dio",
        "flash_freq": "40m",
        "firmware": "pdqminer_cyd_ili9341.bin",
        "partitions": "default_4MB.csv"
    },
    "cyd_st7789": {
        "chip": "esp32",
        "flash_size": "4MB",
        "flash_mode": "dio",
        "flash_freq": "40m",
        "firmware": "pdqminer_cyd_st7789.bin",
        "partitions": "default_4MB.csv"
    }
}
```

### 10.4 CI/CD Integration

GitHub Actions workflow for building and publishing flasher tools:

```yaml
# .github/workflows/release.yml
name: Build and Release

on:
  release:
    types: [published]

jobs:
  build-firmware:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Firmware
        run: |
          cd firmware
          pio run -e cyd_ili9341
          pio run -e cyd_st7789
      - name: Copy to web-flasher
        run: |
          cp firmware/.pio/build/cyd_ili9341/firmware.bin \
             tools/web-flasher/firmware/pdqminer_cyd_ili9341.bin
          cp firmware/.pio/build/cyd_st7789/firmware.bin \
             tools/web-flasher/firmware/pdqminer_cyd_st7789.bin

  deploy-web-flasher:
    needs: build-firmware
    runs-on: ubuntu-latest
    steps:
      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./tools/web-flasher

  publish-python-flasher:
    runs-on: ubuntu-latest
    steps:
      - name: Publish to PyPI
        run: |
          cd tools/python-flasher
          pip install build twine
          python -m build
          twine upload dist/*
```

---

## 11. PDQManager - Fleet Management Application

PDQManager is a **platform-independent Python application** that runs on a PC to monitor and configure multiple PDQminer devices on a network. By running the management UI on the PC rather than the miners, hashrate is preserved.

### 11.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         User's PC                                    │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                      PDQManager                                │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐   │  │
│  │  │  Discovery  │  │  Web Server  │  │  Device Manager    │   │  │
│  │  │  (mDNS/UDP) │  │  (Flask)     │  │  (REST client)     │   │  │
│  │  └──────┬──────┘  └──────┬───────┘  └─────────┬──────────┘   │  │
│  └─────────┼────────────────┼───────────────────┼───────────────┘  │
│            │                │                   │                   │
│    ┌───────▼────────┐ ┌─────▼─────┐   ┌────────▼────────┐         │
│    │  Browser UI    │ │  REST API │   │  Device Comms   │         │
│    │  (React/Vue)   │ │  /api/*   │   │  HTTP + Auth    │         │
│    └────────────────┘ └───────────┘   └────────┬────────┘         │
└──────────────────────────────────────────────────┼─────────────────┘
                                                   │ WiFi Network
        ┌──────────────────────────────────────────┼─────────────────┐
        │                                          │                 │
   ┌────▼────┐    ┌─────────┐    ┌─────────┐    ┌─▼───────┐         │
   │ PDQminer│    │ PDQminer│    │ PDQminer│    │ PDQminer│         │
   │   #1    │    │   #2    │    │   #3    │    │   #4    │         │
   │ REST API│    │ REST API│    │ REST API│    │ REST API│         │
   └─────────┘    └─────────┘    └─────────┘    └─────────┘         │
        └────────────────────────────────────────────────────────────┘
```

### 11.2 Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Runs on PC, not miners | Preserves 100% hashrate on ESP32 |
| Python + Flask | Cross-platform, easy to install, single executable possible |
| Local web server | Universal browser UI, no desktop framework needed |
| REST API on miners | Lightweight, simple to implement on ESP32 |
| mDNS discovery | Zero-config device detection |
| Per-device passwords | Secure fleet management |

### 11.3 PDQManager Application (PC-side)

#### 11.3.1 Features

- **Network Discovery**: Auto-detect PDQminer devices via mDNS (`_pdqminer._tcp.local`)
- **Device Monitoring**: Real-time hashrate, temperature, uptime, share count
- **Fleet Dashboard**: Aggregate statistics across all miners
- **Configuration**: Change pool, wallet, WiFi settings remotely
- **Password Management**: Secure device authentication
- **Alerts**: Offline detection, low hashrate warnings
- **Export**: CSV/JSON data export for analysis

#### 11.3.2 File Structure

```
tools/pdqmanager/
├── pdqmanager.py           # Entry point - launches web server
├── requirements.txt
├── setup.py                # pip installable
├── pdqmanager/
│   ├── __init__.py
│   ├── app.py              # Flask application
│   ├── discovery.py        # mDNS/UDP device scanner
│   ├── device.py           # Device communication client
│   ├── models.py           # Data models
│   ├── api/
│   │   ├── __init__.py
│   │   ├── devices.py      # /api/devices endpoints
│   │   ├── config.py       # /api/config endpoints
│   │   └── stats.py        # /api/stats endpoints
│   ├── static/
│   │   ├── css/
│   │   └── js/
│   └── templates/
│       ├── index.html      # Dashboard
│       ├── device.html     # Single device view
│       └── settings.html   # App settings
└── tests/
    ├── test_discovery.py
    ├── test_device.py
    └── test_api.py
```

#### 11.3.3 Dependencies

```
# requirements.txt
flask>=3.0
flask-cors>=4.0
zeroconf>=0.131     # mDNS discovery
requests>=2.31      # HTTP client for device API
pydantic>=2.0       # Data validation
apscheduler>=3.10   # Periodic polling
cryptography>=41.0  # Password hashing
```

#### 11.3.4 CLI Interface

```bash
# Start PDQManager (opens browser automatically)
pdqmanager

# Start on specific port
pdqmanager --port 8080

# Start without opening browser
pdqmanager --no-browser

# Scan network only (no web server)
pdqmanager scan

# Export all miner data to JSON
pdqmanager export --format json --output miners.json
```

#### 11.3.5 Web UI Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Dashboard with all miners |
| `/device/<id>` | GET | Single device detail page |
| `/settings` | GET | Application settings |
| `/api/devices` | GET | List all discovered devices |
| `/api/devices/<id>` | GET | Single device status |
| `/api/devices/<id>/config` | GET/PUT | Device configuration |
| `/api/devices/<id>/auth` | POST | Authenticate with device |
| `/api/scan` | POST | Trigger network scan |
| `/api/stats` | GET | Aggregate fleet statistics |

#### 11.3.6 Device Data Model

```python
from pydantic import BaseModel
from typing import Optional
from datetime import datetime

class DeviceStatus(BaseModel):
    device_id: str
    name: str
    ip_address: str
    firmware_version: str
    uptime_seconds: int
    hashrate_khs: float
    hashrate_avg_khs: float
    shares_accepted: int
    shares_rejected: int
    best_difficulty: str
    temperature_c: Optional[float]
    free_heap: int
    pool_connected: bool
    pool_host: str
    last_seen: datetime
    authenticated: bool

class DeviceConfig(BaseModel):
    wifi_ssid: str
    pool_primary_host: str
    pool_primary_port: int
    pool_backup_host: Optional[str]
    pool_backup_port: Optional[int]
    wallet_address: str
    worker_name: str
    display_mode: int

class FleetStats(BaseModel):
    total_devices: int
    online_devices: int
    total_hashrate_khs: float
    total_shares_accepted: int
    avg_hashrate_khs: float
```

#### 11.3.7 Discovery Implementation

```python
from zeroconf import ServiceBrowser, Zeroconf, ServiceListener
from typing import Dict, Callable

class PDQMinerDiscovery(ServiceListener):
    SERVICE_TYPE = "_pdqminer._tcp.local."

    def __init__(self, on_device_found: Callable):
        self.devices: Dict[str, dict] = {}
        self.on_device_found = on_device_found
        self.zeroconf = Zeroconf()
        self.browser = None

    def start(self):
        self.browser = ServiceBrowser(self.zeroconf, self.SERVICE_TYPE, self)

    def stop(self):
        if self.browser:
            self.browser.cancel()
        self.zeroconf.close()

    def add_service(self, zc: Zeroconf, type_: str, name: str):
        info = zc.get_service_info(type_, name)
        if info:
            device = {
                "id": name.split(".")[0],
                "ip": ".".join(str(b) for b in info.addresses[0]),
                "port": info.port,
            }
            self.devices[device["id"]] = device
            self.on_device_found(device)

    def remove_service(self, zc: Zeroconf, type_: str, name: str):
        device_id = name.split(".")[0]
        self.devices.pop(device_id, None)
```

### 11.4 Device Management API (Miner-side)

Lightweight REST API on each PDQminer for remote monitoring and configuration.

#### 11.4.1 Design Constraints

| Constraint | Limit | Rationale |
|------------|-------|-----------|
| Max concurrent connections | 2 | Preserve memory for mining |
| Request timeout | 5 seconds | Prevent blocking mining |
| Response size | < 2KB | Minimize heap usage |
| Polling interval | >= 5 seconds | Rate limiting |

#### 11.4.2 API Endpoints

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/status` | GET | No | Basic status (hashrate, uptime) |
| `/api/info` | GET | No | Device info (version, MAC) |
| `/api/config` | GET | Yes | Current configuration |
| `/api/config` | PUT | Yes | Update configuration |
| `/api/auth` | POST | No | Authenticate (returns token) |
| `/api/restart` | POST | Yes | Reboot device |
| `/api/reset` | POST | Yes | Factory reset |

#### 11.4.3 Authentication Flow

```
┌─────────────┐                    ┌─────────────┐
│  PDQManager │                    │  PDQminer   │
└──────┬──────┘                    └──────┬──────┘
       │  POST /api/auth                  │
       │  {"password": "..."}             │
       │─────────────────────────────────►│
       │  200 {"token": "abc...",         │
       │       "expires_in": 3600}        │
       │◄─────────────────────────────────│
       │  GET /api/config                 │
       │  Authorization: Bearer abc...    │
       │─────────────────────────────────►│
       │  200 {config data}               │
       │◄─────────────────────────────────│
```

#### 11.4.4 Response Formats

**GET /api/status** (no auth)
```json
{
    "device_id": "PDQ_A1B2C3",
    "hashrate_khs": 520.5,
    "uptime_s": 3600,
    "shares_accepted": 42,
    "shares_rejected": 1,
    "pool_connected": true,
    "free_heap": 45000
}
```

**GET /api/config** (auth required)
```json
{
    "wifi_ssid": "HomeNetwork",
    "pool1_host": "public-pool.io",
    "pool1_port": 21496,
    "pool2_host": "solo.ckpool.org",
    "pool2_port": 3333,
    "wallet": "bc1q...",
    "worker": "miner1"
}
```

**POST /api/auth**
```json
{"password": "MySecurePassword123!"}
```
Response:
```json
{"token": "eyJhbGci...", "expires_in": 3600}
```

### 11.5 Device Password System

#### 11.5.1 Password Requirements

| Requirement | Specification |
|-------------|---------------|
| Minimum length | 8 characters |
| Maximum length | 64 characters |
| Character set | ASCII printable (0x20-0x7E) |
| Storage | PBKDF2-SHA256 hash in NVS (encrypted) |
| Default | None (must be set on first boot) |

#### 11.5.2 Initial Password Setup

During first boot configuration portal, user **must** set a device password:

```html
<div class="form-group">
    <label for="device_pass">Device Management Password *</label>
    <input type="password" id="device_pass" name="device_pass"
           required minlength="8" maxlength="64">
    <small>8-64 characters. Required for remote management.</small>
</div>
```

#### 11.5.3 Password Storage Implementation

```c
#define PDQ_PASSWORD_MIN_LEN    8
#define PDQ_PASSWORD_MAX_LEN    64
#define PDQ_PASSWORD_HASH_LEN   32
#define PBKDF2_ITERATIONS       10000

typedef struct {
    uint8_t Hash[PDQ_PASSWORD_HASH_LEN];
    uint8_t Salt[16];
    uint32_t Iterations;
} PasswordStore_t;

int32_t PdqPasswordSet(const char *p_Password);
bool PdqPasswordVerify(const char *p_Password);
bool PdqPasswordIsSet(void);
```

### 11.6 mDNS Service Advertisement

```c
#define PDQ_MDNS_SERVICE_TYPE   "_pdqminer"
#define PDQ_MDNS_PROTO          "_tcp"

int32_t PdqMdnsInit(void)
{
    mdns_init();
    char Hostname[32];
    snprintf(Hostname, sizeof(Hostname), "pdqminer-%s", PdqGetDeviceIdShort());
    mdns_hostname_set(Hostname);
    mdns_service_add(NULL, PDQ_MDNS_SERVICE_TYPE, PDQ_MDNS_PROTO, 80, NULL, 0);

    mdns_txt_item_t Txt[] = {
        {"version", FIRMWARE_VERSION},
        {"hardware", BOARD_NAME},
        {"id", PdqGetDeviceId()}
    };
    mdns_service_txt_set(PDQ_MDNS_SERVICE_TYPE, PDQ_MDNS_PROTO, Txt, 3);
    return 0;
}
```

### 11.7 Security Considerations

| Threat | Mitigation |
|--------|------------|
| Password brute force | Rate limiting (3 attempts/minute), exponential backoff |
| Token theft | Short expiry (1 hour), single active session |
| Network sniffing | HTTPS optional, sensitive data hashed |
| Unauthorized access | All config endpoints require authentication |
| DoS attacks | Connection limits, request size limits |

---

## 12. Implementation Status

### 12.1 Completed Components

| Component          | File                           | Status       | Notes                                                             |
| --------------------| --------------------------------| --------------| -------------------------------------------------------------------|
| SHA256 Engine      | `src/core/sha256_engine.c`     | **Complete** | SW: 64-round unrolled, midstate, W pre-computation. HW: ESP32 SHA peripheral with overlap + START→CONTINUE chaining, ~369 cyc/nonce, boot-time diagnostic |
| Mining Task        | `src/core/mining_task.c`       | **Complete** | Dual-task: HW (Core 0, ~650 KH/s) + SW (Core 1, ~46 KH/s) = ~700 KH/s. Nonce split 7/8 HW + 1/8 SW |
| Board HAL          | `src/hal/board_hal.c`          | **Complete** | WDT, temp, heap, chip ID using ESP-IDF APIs                       |
| Benchmark Firmware | `src/main_benchmark.cpp`       | **Complete** | Single/dual-core hashrate measurement                             |
| Stratum Client     | `src/stratum/stratum_client.c` | **Complete** | Full V1 protocol, job building, merkle root computation           |
| WiFi Manager       | `src/network/wifi_manager.cpp` | **Complete** | Captive portal, NVS config saving                                 |
| Config Manager     | `src/config/config_manager.c`  | **Complete** | NVS storage for all settings                                      |
| Main Entry Point   | `src/main.cpp`                 | **Complete** | Full initialization sequence with timeout handling                |
| Build System       | `platformio.ini`               | **Complete** | ESP32 (CYD, headless, debug, benchmark), ESP8266, native          |
| Display Driver     | `src/display/display_driver.cpp` | **Complete** | TFT_eSPI for ILI9341/ST7789, mining stats screen                 |

### 12.2 Stub Components (Awaiting Implementation)

| Component | File | Phase |
|-----------|------|-------|
| Device API | `src/api/device_api.c` | Phase D |

### 12.3 Verified Builds

| Environment | RAM | Flash | Status |
|-------------|-----|-------|--------|
| cyd_ili9341 | 16.4% | 62.8% | SUCCESS |
| cyd_st7789 | 16.4% | 62.8% | SUCCESS |
| esp32_headless | 16.3% | 61.3% | SUCCESS |
| benchmark | 6.5% | 22.0% | SUCCESS |
| esp8266_headless | TBD | TBD | Added (Session 26) |

### 12.4 Code Review Status (Round 11 - Post HW SHA Optimization)

| Component      | Accuracy   | Security | Optimization  | Notes                                                |
| ----------------| -----------| ----------| --------------| ------------------------------------------------------|
| SHA256 Engine  | **100%**   | N/A      | **HW+SW**     | HW SHA peripheral at 413 cyc/nonce (overlap), SW midstate+unrolled, GCC push/pop_options |
| Mining Task    | **100%**   | **100%** | **HW+SW**     | Dual-task: MiningTaskHw (Core 0) + MiningTaskCore1 (Core 1), nonce overflow fixed |
| Stratum Client | **100%**   | **100%** | Good          | Protocol compliance verified, extranonce handling    |
| WiFi Manager   | **100%**   | Secure   | Good          | Form parsing, NVS save, strncpy null-term            |
| Config Manager | **100%**   | Good     | Good          | Magic validation                                     |
| Main           | **100%**   | Good     | Good          | HW diagnostic on boot, non-destructive debug print, share submission |
| Display Driver | **100%**   | N/A      | Good          | TFT_eSPI, ILI9341 with LOAD_GLCD/LOAD_FONT2, headless stubs |

### 12.5 API Changes Log

| Version | Change | Reason |
|---------|--------|--------|
| 0.1.0 | `PdqStratumSubmitShare()` added `Extranonce2` parameter | Required for correct share submission |
| 0.1.0 | `PdqMiningJob_t.JobId` changed from `uint64_t` to `char[65]` | Stratum uses string job IDs |
| 0.1.0 | Added `PdqShareInfo_t` struct | Store share data for submission |
| 0.1.0 | Added `PdqMiningHasShare()` and `PdqMiningGetShare()` | Share queue interface |
| 0.1.0 | Added `PdqStratumGetExtranonce2Size()` | Get pool-specified extranonce2 length |
| 0.1.0 | Added `MiningState_t.JobVersion` | Detect new jobs and restart mining |
| 0.1.0 | Changed `WriteBe32` to `WriteLe32` for nonce | Bitcoin protocol compliance |
| 0.1.0 | Added `Sha256TransformW()` | Pre-expanded W array input for optimization |

### 12.6 Critical Bug Fixes (Session 18)

| Bug | Location | Impact | Fix |
|-----|----------|--------|-----|
| Uninitialized padding memory | sha256_engine.c:282-287 | Hash computation could fail randomly | Extended memset to cover bytes 33-63 |
| Nonce written big-endian | sha256_engine.c:275 | ALL shares would be rejected | Changed to WriteLe32 |
| Stale job mining | mining_task.c | Mining outdated jobs, wasting cycles | Added JobVersion tracking |
| Hardcoded extranonce2 size | main.cpp:137 | Incompatibility with some pools | Use PdqStratumGetExtranonce2Size() |

### 12.7 SHA256 Optimization Pass 1 (Session 19)

| Optimization | Description | Savings |
|--------------|-------------|---------|
| No byte conversion | Hash output feeds directly to next hash input | ~16 WriteBe32/ReadBe32 per nonce |
| Pre-computed W1[16-18] | W1[16], W1[17], partial W1[18] computed once | 3 SIG operations per nonce |
| Early rejection | Check FinalState[7] before full target comparison | ~7 comparisons per nonce (avg) |
| Optimized transform | `Sha256TransformW()` takes W array directly | 16 ReadBe32 per transform |
| Reduced loop start | W1 expansion starts at index 19 | 3 fewer iterations per nonce |

### 12.8 SHA256 Optimization Pass 2 (Session 22)

| Optimization | Description | Savings |
|--------------|-------------|---------|
| W2 constant pre-computation | SIG1(256), SIG0(0x80000000) pre-computed | 2 SIG operations per nonce |
| W2[16-24] explicit expansion | Eliminates zero additions and loop overhead | ~8 fewer iterations per nonce |
| W1 array reuse | Single W1[64] array, only W1[3] updated per nonce | ~15 memory copies per nonce |
| Reduced W2 loop | W2 expansion starts at index 25 instead of 16 | 9 fewer iterations per nonce |

**Pre-computed W2 Constants:**
- `W2_SIG1_15 = SIG1(256) = 0x00A00000`
- `W2_SIG0_8 = SIG0(0x80000000) = 0x11002000` (corrected in Session 23)

### 12.9 Display Driver Implementation (Session 21)

| Feature | Description |
|---------|-------------|
| TFT_eSPI Integration | Uses Bodmer's TFT_eSPI library for ILI9341/ST7789 |
| Mining Stats Screen | Hashrate, shares, blocks, temperature, uptime |
| Auto-formatting | Hashrate displayed as H/s, KH/s, or MH/s |
| Color-coded Temp | White (<55C), Orange (55-70C), Red (>70C) |
| Rate Limiting | Display updates throttled to 500ms minimum |
| Headless Support | All functions compile to no-ops when PDQ_HEADLESS defined |

### 12.10 Memory Analysis (Session 22)

| Component | RAM Usage | Notes |
|-----------|-----------|-------|
| IotWebConf (s_Ctx) | 5,884 bytes | Config portal context |
| WiFi (g_cnxMgr) | 3,800 bytes | Connection manager |
| Stratum Job | 1,144 bytes | Job buffer with coinbase |
| DNS Table | 1,184 bytes | LWIP DNS cache |
| SHA256 K constants | 640 bytes | DRAM for cache locality |
| **Total BSS** | 28,449 bytes | 8.7% of available |
| **Total Data** | 127,236 bytes | Initialized data |

### 12.11 Code Review Round 7 - Critical Bug Fix (Session 23)

**Critical Bug Found and Fixed:**

| Issue | Details |
|-------|---------|
| Location | `src/core/sha256_engine.c:360` |
| Constant | `W2_SIG0_8` |
| Incorrect | `0x00800001` |
| Correct | `0x11002000` |

**Mathematical Verification:**
```
SIG0(x) = ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3)

For x = 0x80000000:
  ROTR(0x80000000, 7)  = 0x01000000
  ROTR(0x80000000, 18) = 0x00002000
  0x80000000 >> 3      = 0x10000000

Result: 0x01000000 ^ 0x00002000 ^ 0x10000000 = 0x11002000
```

**Security Review Results:**
| Category | Status |
|----------|--------|
| Buffer safety | PASS - strncpy/snprintf throughout |
| Input validation | PASS - NULL checks on all public APIs |
| Password handling | PASS - proper bounds checking |
| Integer overflow | PASS - no unsafe arithmetic |

### 12.12 Critical Bug Fixes (Session 27)

User reported 54 H/s on ESP32-2432S028R ST7789 device. Root cause analysis identified three critical bugs.

| Bug | Location | Impact | Fix |
|-----|----------|--------|-----|
| Debug print consuming job notifications | `main.cpp:147-148` | Jobs rarely processed (root cause of 54 H/s) | Changed destructive `PdqStratumHasNewJob()` to non-destructive `PdqStratumGetState()` |
| Nonce overflow in mining loop | `sha256_engine.c:443-444` | Infinite loop when NonceEnd=0xFFFFFFFF | Changed to `for(;;)` with explicit `if (Nonce == NonceEnd) break` |
| Nonce overflow in mining task | `mining_task.c:91-125, 151-185` | Infinite loop in batch outer loop | Changed to `while` loop with overflow-safe boundary check |

### 12.13 SHA256 Optimization Pass 3 (Session 27)

| Optimization | Description | Savings |
|--------------|-------------|---------|
| W1 constant elimination | W1_4-W1_15 replaced with compile-time constants (padding) | 12 ReadBe32 per nonce |
| SIG0/SIG1 pre-computation | SIG0(0x80000000), SIG0(0x280), SIG1(0x280) | 3 SIG operations per nonce |
| Zero-term elimination | Removed additions of zero-valued W1_5-W1_14, SIG0(0), SIG1(0) | ~12 additions per nonce |
| KW1 pre-computed constants | K[i]+W[i] pre-computed for rounds 4-15 | 12 additions per nonce |
| SHA256_ROUND_KW macro | Combined K+W variant for constant rounds | Cleaner hot path |

**Pre-computed KW1 Constants:**
```c
static const uint32_t KW1_4  = 0x3956c25b + 0x80000000;  /* K[4] + W1_4 */
static const uint32_t KW1_5  = 0x59f111f1;               /* K[5] + 0 */
/* ... KW1_6 through KW1_14 = K[i] + 0 ... */
static const uint32_t KW1_15 = 0xc19bf174 + 0x280;       /* K[15] + W1_15 */
```

**Simplified W1 Pre-computation (zero terms eliminated):**
```c
uint32_t W1_20 = SIG1(W1_18) + W1_4;       /* W1_13=0, SIG0(W1_5)=0 */
uint32_t W1_21 = SIG1(W1_19);              /* all zero terms */
uint32_t W1_22 = SIG1(W1_20) + W1_15;      /* SIG0(W1_7)=0, W1_6=0 */
uint32_t W1_30 = SIG1(W1_28) + W1_23 + SIG0_W1_15;  /* first non-zero SIG0 */
```

### 12.14 Hardware SHA256 Engine (Sessions 29-30)

ESP32-D0 hardware SHA256 acceleration via the SHA peripheral at `SHA_TEXT_BASE` (0x3FF03000).

**Hardware Characterization (via `PdqSha256HwDiagnostic()`):**

| Property | Result |
|----------|--------|
| LOAD direction | Internal state → SHA_TEXT (read-only, cannot write midstate) |
| Auto-store after START/CONTINUE | No (explicit LOAD required) |
| SHA_TEXT preservation after operations | Yes (contents survive START/CONTINUE/LOAD) |
| LOAD preservation of [8:15] | Yes (LOAD only overwrites [0:7]) |
| Overlap writes during START | **SAFE** (engine latches at trigger time) |
| Overlap writes during CONTINUE | **UNSAFE** (engine reads registers progressively) |
| START timing | 627 CPU cycles |
| CONTINUE timing | 627 CPU cycles |
| LOAD timing | 562 CPU cycles |

**Midstate Caching Investigation:**
- ESP32-D0 has **no `SHA_H_BASE` register** (only exists on ESP32-S2/S3/C3)
- `SOC_SHA_SUPPORT_RESUME` is NOT defined for ESP32 in ESP-IDF
- Espressif docs: "not possible to restore a SHA digest state into the engine"
- Each nonce requires a full 3-block SHA256d sequence (no shortcuts)

**Performance:**

| Metric | Value |
|--------|-------|
| HW cycles/nonce | 413 |
| HW hashrate | 581 KH/s |
| SW hashrate (Core 1) | ~46 KH/s |
| Combined | **627 KH/s** |

### 12.15 Overlap Optimization (Session 30)

Overlapping SHA_TEXT register writes with SHA engine computation.

| Optimization | Description | Savings |
|--------------|-------------|---------|
| Block1 fill during Block0 START | 16 APB writes hidden during 627-cycle START | 16 register writes |
| Block0 half-fill during double-hash START | 8 APB writes hidden during 627-cycle START | 8 register writes |
| 2-write reduced padding | Only TextNV[8] and TextNV[15] need updating between iterations | 14 register writes |
| SHA_TEXT preservation | Reuse block0 SHA_TEXT[8:15] from previous overlap fill | 8 register writes |

**IRAM Test Result:**
- Placing `PdqSha256MineBlockHw()` in IRAM **regressed** performance (538 cyc vs 413 cyc)
- Root cause: Flash cache already efficient for hot loops; IRAM likely caused bus contention
- Decision: Keep HW mining code in flash, use -O2 only

**Commits:**
- `23617e0`: HW SHA256 engine implementation (+1190/-566 lines, 11 files)
- `7286523`: Overlap optimization (+290/-28 lines, 4 files)

---

*This document is a living specification and will be updated as the design evolves.*
