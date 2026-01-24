# PDQminer Coding Standards

> **Version**: 1.0.0  
> **Last Updated**: 2025-01-XX  
> **Applies To**: All PDQminer source code

---

## 1. Naming Conventions

### 1.1 PascalCase Requirements

PDQminer uses **PascalCase** as the primary naming convention across all code elements.

| Element | Convention | Example |
|---------|------------|---------|
| Functions | PascalCase | `ComputeSha256Hash()` |
| Variables (global) | PascalCase | `GlobalHashRate` |
| Variables (local) | PascalCase | `NonceValue` |
| Structs | PascalCase | `MiningJobData` |
| Enums | PascalCase | `MinerState` |
| Enum Values | PascalCase | `MinerStateIdle` |
| Macros/Constants | SCREAMING_SNAKE | `MAX_NONCE_VALUE` |
| File Names | snake_case | `sha256_engine.c` |
| Typedefs | PascalCase + `_t` suffix | `HashResult_t` |

### 1.2 Prefixes

| Prefix | Usage | Example |
|--------|-------|---------|
| `Pdq` | Public API functions | `PdqMinerStart()` |
| `g_` | Global variables | `g_CurrentHashRate` |
| `p_` | Pointer parameters | `p_JobData` |
| `s_` | Static variables | `s_MidstateCache` |

---

## 2. File Organization

### 2.1 Header File Template

```c
/**
 * @file    module_name.h
 * @brief   Brief description of the module
 * @author  PDQminer Team
 * @version 1.0.0
 * @date    YYYY-MM-DD
 *
 * @details
 * Detailed description of the module's purpose and functionality.
 *
 * @note    Any important notes for users of this module.
 *
 * @copyright
 * SPDX-License-Identifier: MIT
 */

#ifndef PDQMINER_MODULE_NAME_H
#define PDQMINER_MODULE_NAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * INCLUDES
 * ========================================================================= */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * MACROS AND CONSTANTS
 * ========================================================================= */

#define MODULE_CONSTANT_VALUE    (100U)

/* ============================================================================
 * TYPE DEFINITIONS
 * ========================================================================= */

/**
 * @brief   Description of the structure
 */
typedef struct {
    uint32_t    FieldOne;       /**< Description of FieldOne */
    uint8_t     FieldTwo;       /**< Description of FieldTwo */
} ModuleData_t;

/* ============================================================================
 * PUBLIC FUNCTION PROTOTYPES
 * ========================================================================= */

/**
 * @brief   Brief description of function
 * @param   p_Data  Pointer to data structure
 * @return  Status code (0 = success)
 */
int32_t ModuleFunctionName(ModuleData_t *p_Data);

#ifdef __cplusplus
}
#endif

#endif /* PDQMINER_MODULE_NAME_H */
```

### 2.2 Source File Template

```c
/**
 * @file    module_name.c
 * @brief   Brief description of the module implementation
 * @author  PDQminer Team
 * @version 1.0.0
 * @date    YYYY-MM-DD
 */

/* ============================================================================
 * INCLUDES
 * ========================================================================= */

#include "module_name.h"
#include <string.h>

/* ============================================================================
 * PRIVATE MACROS
 * ========================================================================= */

#define PRIVATE_CONSTANT    (42U)

/* ============================================================================
 * PRIVATE TYPE DEFINITIONS
 * ========================================================================= */

typedef struct {
    uint32_t InternalField;
} PrivateData_t;

/* ============================================================================
 * PRIVATE VARIABLES
 * ========================================================================= */

static PrivateData_t s_PrivateData;

/* ============================================================================
 * PRIVATE FUNCTION PROTOTYPES
 * ========================================================================= */

static void HelperFunction(uint32_t Value);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ========================================================================= */

/**
 * @brief   Implementation of public function
 */
int32_t ModuleFunctionName(ModuleData_t *p_Data)
{
    if (p_Data == NULL) {
        return -1;
    }

    HelperFunction(p_Data->FieldOne);
    return 0;
}

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ========================================================================= */

/**
 * @brief   Internal helper function
 */
static void HelperFunction(uint32_t Value)
{
    s_PrivateData.InternalField = Value;
}
```

---

## 3. Code Style

### 3.1 Indentation and Spacing

- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Maximum 100 characters
- **Braces**: Allman style (opening brace on new line for functions, K&R for control structures)

```c
/* Function definition - Allman style */
int32_t PdqComputeHash(uint8_t *p_Data, uint32_t Length)
{
    /* Control structure - K&R style */
    if (p_Data == NULL) {
        return -1;
    }

    for (uint32_t Index = 0; Index < Length; Index++) {
        ProcessByte(p_Data[Index]);
    }

    return 0;
}
```

### 3.2 Comments

#### Block Comments
```c
/*
 * Multi-line block comment for explaining
 * complex logic or algorithms.
 */
```

#### Inline Comments
```c
uint32_t Nonce = 0;  /* Starting nonce value */
```

#### Section Dividers
```c
/* ============================================================================
 * SECTION NAME
 * ========================================================================= */
```

### 3.3 Documentation Requirements

**Every function must include:**
1. `@brief` - One-line description
2. `@param` - For each parameter
3. `@return` - Return value description
4. `@note` - Optional: important usage notes
5. `@warning` - Optional: potential pitfalls

```c
/**
 * @brief   Computes SHA256 double hash of Bitcoin block header
 *
 * @param   p_Header    Pointer to 80-byte block header
 * @param   p_HashOut   Pointer to 32-byte output buffer
 *
 * @return  0 on success, negative error code on failure
 *
 * @note    This function uses hardware acceleration when available.
 * @warning p_HashOut must be at least 32 bytes allocated.
 */
int32_t PdqSha256Double(const uint8_t *p_Header, uint8_t *p_HashOut);
```

---

## 4. ESP32-Specific Guidelines

### 4.1 Memory Attributes

```c
/* Place in IRAM for fast execution (hot path) */
IRAM_ATTR void PdqHashLoop(void);

/* Place in DRAM for fast data access */
DRAM_ATTR static uint32_t s_MidstateBuffer[16];

/* Place constant data in flash to save RAM */
static const uint8_t RODATA_ATTR s_LookupTable[256] = { ... };
```

### 4.2 Core Pinning

```c
/* Pin hashing tasks to specific core */
xTaskCreatePinnedToCore(
    HashingTask,            /* Task function */
    "HashCore0",            /* Task name */
    HASH_TASK_STACK_SIZE,   /* Stack size */
    NULL,                   /* Parameters */
    HASH_TASK_PRIORITY,     /* Priority */
    &g_HashTaskHandle,      /* Task handle */
    0                       /* Core ID (0 or 1) */
);
```

### 4.3 Watchdog Management

```c
/* Feed watchdog in long-running loops */
void PdqHashingLoop(void)
{
    uint32_t IterationCount = 0;

    while (g_MiningActive) {
        PerformHashIteration();
        IterationCount++;

        /* Feed watchdog every N iterations */
        if ((IterationCount & 0xFFF) == 0) {
            esp_task_wdt_reset();
            vTaskDelay(1);  /* Minimal yield */
        }
    }
}
```

---

## 5. Performance-Critical Code

### 5.1 Hot Path Requirements

Code in the hashing hot path must:

1. **No dynamic allocation** - Use static or stack allocation
2. **No blocking calls** - Avoid mutexes in inner loops
3. **Minimize branching** - Use lookup tables where possible
4. **Cache-friendly access** - Sequential memory access patterns
5. **Inline critical functions** - Use `FORCE_INLINE` macro

```c
#define FORCE_INLINE __attribute__((always_inline)) inline

FORCE_INLINE static uint32_t RotateRight(uint32_t Value, uint32_t Bits)
{
    return (Value >> Bits) | (Value << (32 - Bits));
}
```

### 5.2 SHA256 Round Optimization

```c
/* Unrolled SHA256 round - no loop overhead */
#define SHA256_ROUND(A, B, C, D, E, F, G, H, K, W) \
    do { \
        uint32_t Temp1 = H + Sigma1(E) + Ch(E, F, G) + K + W; \
        uint32_t Temp2 = Sigma0(A) + Maj(A, B, C); \
        H = G; G = F; F = E; E = D + Temp1; \
        D = C; C = B; B = A; A = Temp1 + Temp2; \
    } while (0)
```

---

## 6. Error Handling

### 6.1 Return Codes

```c
typedef enum {
    PdqErrorNone            =  0,   /**< Success */
    PdqErrorInvalidParam    = -1,   /**< Invalid parameter */
    PdqErrorNoMemory        = -2,   /**< Memory allocation failed */
    PdqErrorTimeout         = -3,   /**< Operation timed out */
    PdqErrorBusy            = -4,   /**< Resource busy */
    PdqErrorNotInitialized  = -5,   /**< Module not initialized */
} PdqError_t;
```

### 6.2 Assertion Macros

```c
#define PDQ_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            ESP_LOGE(TAG, "Assertion failed: %s, line %d", __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define PDQ_RETURN_IF_NULL(ptr, retval) \
    do { \
        if ((ptr) == NULL) { \
            return (retval); \
        } \
    } while (0)
```

---

## 7. Version Control

### 7.1 Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:** `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `chore`

**Example:**
```
perf(sha256): optimize midstate computation

- Precompute first 64 bytes of message schedule
- Reduce per-nonce computation by 50%
- Measured improvement: 340 KH/s → 680 KH/s

Closes #42
```

---

## 8. Testing Requirements

### 8.1 Unit Test Naming

```c
void Test_Sha256_ValidInput_ReturnsCorrectHash(void);
void Test_Sha256_NullPointer_ReturnsError(void);
void Test_Stratum_ParseJob_ValidJson_Success(void);
```

### 8.2 Test Coverage

- All public functions: 100% coverage required
- Critical paths (SHA256, Stratum): Branch coverage required
- Edge cases: Null pointers, boundary values, overflow conditions

---

## 9. Security Best Practices

### 9.1 Input Validation (Mandatory)

All external input MUST be validated before use:

```c
/**
 * @brief   Validate and copy string with bounds checking
 * @return  0 on success, negative error on validation failure
 */
int32_t PdqValidateString(char *p_Dest, const char *p_Src, size_t MaxLen,
                          PdqCharValidator_t Validator)
{
    PDQ_RETURN_IF_NULL(p_Dest, PdqErrorInvalidParam);
    PDQ_RETURN_IF_NULL(p_Src, PdqErrorInvalidParam);

    size_t Len = strnlen(p_Src, MaxLen + 1);
    if (Len > MaxLen) {
        return PdqErrorBufferOverflow;
    }

    for (size_t i = 0; i < Len; i++) {
        if (!Validator(p_Src[i])) {
            return PdqErrorInvalidChar;
        }
    }

    memcpy(p_Dest, p_Src, Len);
    p_Dest[Len] = '\0';
    return 0;
}
```

### 9.2 Buffer Overflow Prevention

| Rule | Implementation |
|------|----------------|
| Never use `strcpy()` | Use `strncpy()` or `PdqSafeStrCopy()` |
| Never use `sprintf()` | Use `snprintf()` with size parameter |
| Never use `gets()` | Use `fgets()` with size limit |
| Array bounds | Always validate index before access |
| Stack arrays | Fixed size, never from untrusted input |

### 9.3 Integer Overflow Prevention

```c
/* WRONG - potential overflow */
uint32_t Total = Count * ItemSize;

/* CORRECT - check before multiplication */
if (Count > 0 && ItemSize > UINT32_MAX / Count) {
    return PdqErrorOverflow;
}
uint32_t Total = Count * ItemSize;
```

### 9.4 Memory Safety

```c
/* Always zero-initialize sensitive data after use */
void PdqClearSensitiveData(void *p_Data, size_t Size)
{
    volatile uint8_t *p_Volatile = (volatile uint8_t *)p_Data;
    while (Size--) {
        *p_Volatile++ = 0;
    }
}

/* Use for passwords, keys, tokens */
#define PDQ_CLEAR_SENSITIVE(var) \
    PdqClearSensitiveData(&(var), sizeof(var))
```

### 9.5 Cryptographic Requirements

| Requirement | Implementation |
|-------------|----------------|
| Password hashing | PBKDF2-SHA256, min 100,000 iterations |
| Salt generation | 16 bytes from hardware RNG |
| Token generation | 32 bytes from hardware RNG |
| No hardcoded secrets | All secrets from NVS or user input |

### 9.6 Network Security

```c
/* Validate JSON input size before parsing */
#define PDQ_MAX_JSON_SIZE    4096

/* Reject oversized responses */
if (ResponseLen > PDQ_MAX_JSON_SIZE) {
    ESP_LOGW(TAG, "Response too large: %zu bytes", ResponseLen);
    return PdqErrorBufferOverflow;
}

/* Rate limiting for authentication */
static uint32_t s_AuthAttempts = 0;
static uint32_t s_LastAttemptTime = 0;

#define PDQ_AUTH_MAX_ATTEMPTS   5
#define PDQ_AUTH_LOCKOUT_MS     300000  /* 5 minutes */

bool PdqAuthRateLimitCheck(void)
{
    uint32_t Now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (Now - s_LastAttemptTime > PDQ_AUTH_LOCKOUT_MS) {
        s_AuthAttempts = 0;
    }

    if (s_AuthAttempts >= PDQ_AUTH_MAX_ATTEMPTS) {
        return false;  /* Locked out */
    }

    s_AuthAttempts++;
    s_LastAttemptTime = Now;
    return true;
}
```

---

## 10. Memory Optimization Best Practices

### 10.1 Static Allocation in Hot Paths

```c
/* WRONG - dynamic allocation in mining loop */
void BadMiningLoop(void)
{
    while (Mining) {
        uint8_t *p_Buffer = malloc(128);  /* NEVER DO THIS */
        ComputeHash(p_Buffer);
        free(p_Buffer);
    }
}

/* CORRECT - pre-allocated static buffers */
static DRAM_ATTR uint8_t s_HashBuffer[128];
static DRAM_ATTR uint32_t s_MidstateCache[8];

void GoodMiningLoop(void)
{
    while (Mining) {
        ComputeHash(s_HashBuffer);  /* Zero allocation overhead */
    }
}
```

### 10.2 Memory Placement Attributes

```c
/* Critical hashing code in IRAM for speed */
IRAM_ATTR void PdqSha256Transform(uint32_t *p_State, const uint32_t *p_Block);

/* Frequently accessed data in DRAM */
DRAM_ATTR static Sha256Context_t s_MiningContext;

/* Constant tables in flash to save RAM */
static const RODATA_ATTR uint32_t s_Sha256K[64] = { ... };

/* Stack size optimization - avoid large local arrays */
#define PDQ_HASH_TASK_STACK    4096  /* Measured minimum + margin */
```

### 10.3 Cache-Friendly Data Layout

```c
/* WRONG - scattered access pattern */
typedef struct {
    char     Name[64];      /* Rarely accessed */
    uint32_t Nonce;         /* Hot path */
    char     Description[256];
    uint32_t Target[8];     /* Hot path */
} BadJobLayout_t;

/* CORRECT - hot data grouped together */
typedef struct {
    /* Hot data first - accessed every hash */
    uint32_t Nonce;
    uint32_t Target[8];
    uint32_t Midstate[8];
    /* Cold data last - accessed once per job */
    char     JobId[64];
} GoodJobLayout_t;
```

### 10.4 String Memory Optimization

```c
/* Use fixed-size buffers, never dynamic strings */
typedef struct {
    char PoolHost[64];      /* Fixed, not char* */
    char Wallet[64];        /* Fixed, not char* */
} Config_t;

/* Use string length fields for variable content */
typedef struct {
    uint8_t Coinbase[256];
    uint16_t CoinbaseLen;   /* Actual length */
} Job_t;
```

### 10.5 Heap Fragmentation Prevention

```c
/* Allocate all buffers at initialization, never during operation */
static bool s_Initialized = false;
static MiningBuffer_t *s_p_Buffers = NULL;

int32_t PdqMinerInit(void)
{
    if (s_Initialized) {
        return PdqErrorAlreadyInit;
    }

    /* Single allocation at startup */
    s_p_Buffers = heap_caps_malloc(sizeof(MiningBuffer_t) * BUFFER_COUNT,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_p_Buffers == NULL) {
        return PdqErrorNoMemory;
    }

    s_Initialized = true;
    return 0;
}

/* Never allocate during mining operation */
```

---

## 11. CPU Optimization Best Practices

### 11.1 Loop Unrolling

```c
/* SHA256 rounds - fully unrolled for maximum speed */
#define SHA256_ROUND_UNROLL_8(i) \
    SHA256_ROUND(A, B, C, D, E, F, G, H, K[i+0], W[i+0]); \
    SHA256_ROUND(H, A, B, C, D, E, F, G, K[i+1], W[i+1]); \
    SHA256_ROUND(G, H, A, B, C, D, E, F, K[i+2], W[i+2]); \
    SHA256_ROUND(F, G, H, A, B, C, D, E, K[i+3], W[i+3]); \
    SHA256_ROUND(E, F, G, H, A, B, C, D, K[i+4], W[i+4]); \
    SHA256_ROUND(D, E, F, G, H, A, B, C, K[i+5], W[i+5]); \
    SHA256_ROUND(C, D, E, F, G, H, A, B, K[i+6], W[i+6]); \
    SHA256_ROUND(B, C, D, E, F, G, H, A, K[i+7], W[i+7]);
```

### 11.2 Branch Reduction

```c
/* WRONG - branch in hot path */
if (HashResult[0] == 0 && HashResult[1] == 0) {
    CheckFullTarget();
}

/* CORRECT - branchless early rejection */
uint32_t EarlyCheck = HashResult[7];  /* Check MSB first */
if ((EarlyCheck >> 16) != 0) {
    continue;  /* 99.998% of hashes rejected here */
}
/* Only check full target for ~0.002% of hashes */
```

### 11.3 Register Optimization

```c
/* Use register keyword for critical variables */
IRAM_ATTR void PdqSha256Transform(uint32_t *p_State, const uint32_t *p_Block)
{
    register uint32_t A = p_State[0];
    register uint32_t B = p_State[1];
    register uint32_t C = p_State[2];
    register uint32_t D = p_State[3];
    register uint32_t E = p_State[4];
    register uint32_t F = p_State[5];
    register uint32_t G = p_State[6];
    register uint32_t H = p_State[7];

    /* ... SHA256 rounds ... */
}
```

### 11.4 Hardware SHA256 Acceleration

```c
/* Use ESP32's hardware SHA256 when available */
#ifdef CONFIG_IDF_TARGET_ESP32S3
    #define PDQ_USE_HW_SHA256  1
    #include "sha/sha_dma.h"
#else
    #define PDQ_USE_HW_SHA256  0
#endif

#if PDQ_USE_HW_SHA256
IRAM_ATTR int32_t PdqSha256Block(const uint8_t *p_Data, uint8_t *p_Hash)
{
    esp_sha_acquire_hardware();
    /* Direct register access for maximum speed */
    sha_hal_hash_block(SHA2_256, p_Data, 64/4, true);
    sha_hal_read_digest(SHA2_256, (uint32_t *)p_Hash);
    esp_sha_release_hardware();
    return 0;
}
#endif
```

---

## 12. Python Tool Standards (PDQFlasher & PDQManager)

### 12.1 Project Structure

```
tools/
├── pdqflasher/
│   ├── __init__.py
│   ├── __main__.py          # Entry point
│   ├── flasher.py           # Main flashing logic
│   ├── detector.py          # Board detection
│   ├── config.py            # Configuration management
│   ├── firmware.py          # Firmware handling
│   └── gui.py               # GUI implementation
├── pdqmanager/
│   ├── __init__.py
│   ├── __main__.py          # Entry point
│   ├── app.py               # Flask application
│   ├── discovery.py         # mDNS discovery
│   ├── device.py            # Device client
│   ├── models.py            # Pydantic models
│   └── api.py               # REST API routes
├── requirements.txt         # Dependencies
├── setup.py                 # Package setup
└── pyproject.toml           # Modern Python config
```

### 12.2 Code Style

- **Formatter**: Black (line length 100)
- **Linter**: Ruff (replaces flake8, isort, pyupgrade)
- **Type Hints**: Required for all functions
- **Docstrings**: Google style

```python
def detect_board(port: str, timeout: float = 5.0) -> BoardInfo | None:
    """Detect ESP32 board type and display controller.

    Args:
        port: Serial port path (e.g., '/dev/ttyUSB0', 'COM3')
        timeout: Detection timeout in seconds

    Returns:
        BoardInfo if detection successful, None otherwise

    Raises:
        SerialException: If port cannot be opened
        TimeoutError: If detection times out

    Example:
        >>> board = detect_board('/dev/ttyUSB0')
        >>> if board:
        ...     print(f"Detected: {board.chip_type}")
    """
```

### 12.3 Error Handling

```python
# Custom exception hierarchy
class PDQError(Exception):
    """Base exception for PDQ tools."""
    pass

class DetectionError(PDQError):
    """Board detection failed."""
    pass

class FlashError(PDQError):
    """Firmware flashing failed."""
    pass

class ConnectionError(PDQError):
    """Device connection failed."""
    pass

# Always use specific exceptions
try:
    board = detect_board(port)
except SerialException as e:
    raise DetectionError(f"Cannot open port {port}: {e}") from e
```

### 12.4 Security in Python Tools

```python
# Never log sensitive data
import logging
logger = logging.getLogger(__name__)

def authenticate(password: str) -> str:
    """Authenticate and return token."""
    # WRONG: logger.info(f"Authenticating with password: {password}")
    logger.info("Authenticating user")  # CORRECT

    # Clear password from memory after use
    token = _auth_request(password)
    password = "x" * len(password)  # Overwrite
    return token

# Validate all user input
from pydantic import BaseModel, field_validator

class DeviceConfig(BaseModel):
    pool_host: str
    pool_port: int
    wallet: str

    @field_validator('pool_port')
    @classmethod
    def validate_port(cls, v: int) -> int:
        if not 1 <= v <= 65535:
            raise ValueError('Port must be 1-65535')
        return v

    @field_validator('wallet')
    @classmethod
    def validate_wallet(cls, v: str) -> str:
        if not v or len(v) > 64:
            raise ValueError('Invalid wallet address')
        if not v.replace('1', '').replace('3', '').replace('bc1', '').isalnum():
            raise ValueError('Wallet contains invalid characters')
        return v
```

### 12.5 Testing Requirements

```python
# test_detector.py
import pytest
from pdqflasher.detector import detect_board, BoardInfo

class TestDetectBoard:
    """Tests for board detection functionality."""

    def test_detect_valid_port_returns_board_info(self, mock_serial):
        """Valid port with ESP32 returns BoardInfo."""
        mock_serial.return_value = b"ESP32-D0"
        result = detect_board("/dev/ttyUSB0")
        assert isinstance(result, BoardInfo)
        assert result.chip_type == "ESP32-D0"

    def test_detect_invalid_port_raises_error(self):
        """Invalid port raises DetectionError."""
        with pytest.raises(DetectionError):
            detect_board("/dev/nonexistent")

    def test_detect_timeout_returns_none(self, mock_serial):
        """Detection timeout returns None."""
        mock_serial.side_effect = TimeoutError()
        result = detect_board("/dev/ttyUSB0", timeout=0.1)
        assert result is None
```

### 12.6 Dependency Management

```toml
# pyproject.toml
[project]
name = "pdqtools"
version = "1.0.0"
requires-python = ">=3.10"
dependencies = [
    "esptool>=4.0",
    "pyserial>=3.5",
    "flask>=3.0",
    "requests>=2.31",
    "pydantic>=2.0",
    "zeroconf>=0.100",
    "rich>=13.0",  # CLI output
]

[project.optional-dependencies]
dev = [
    "pytest>=8.0",
    "pytest-cov>=4.0",
    "black>=24.0",
    "ruff>=0.3",
    "mypy>=1.9",
]
gui = [
    "dearpygui>=2.0",  # Cross-platform GUI
]

[tool.black]
line-length = 100

[tool.ruff]
line-length = 100
select = ["E", "F", "I", "N", "W", "UP"]

[tool.mypy]
strict = true
```

---

## 13. Display Optimization (Minimalistic Mode)

### 13.1 Display Update Strategy

```c
/* Mining mode - minimal display updates to maximize hashrate */
#define PDQ_DISPLAY_UPDATE_INTERVAL_MS  5000  /* Update every 5 seconds */

/* Display update runs on Core 0, mining on Core 1 */
void PdqDisplayTask(void *p_Arg)
{
    TickType_t LastUpdate = 0;

    while (1) {
        TickType_t Now = xTaskGetTickCount();

        /* Only update at intervals - don't steal CPU from mining */
        if ((Now - LastUpdate) >= pdMS_TO_TICKS(PDQ_DISPLAY_UPDATE_INTERVAL_MS)) {
            PdqDisplayUpdateStats();
            LastUpdate = Now;
        }

        /* Long sleep - give mining core maximum time */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 13.2 Minimalistic Display Content

```c
/* Only essential information - no animations or graphics */
typedef struct {
    uint32_t HashRate;      /* KH/s */
    uint32_t Shares;        /* Accepted shares */
    uint32_t Uptime;        /* Seconds */
    bool     Connected;     /* Pool connection */
} MinimalStats_t;

/* Simple text-only display update */
void PdqDisplayMinimalUpdate(const MinimalStats_t *p_Stats)
{
    char Line1[32], Line2[32];

    snprintf(Line1, sizeof(Line1), "%u KH/s", p_Stats->HashRate);
    snprintf(Line2, sizeof(Line2), "Shares: %u", p_Stats->Shares);

    /* Single SPI transaction - minimal bus usage */
    PdqDisplayClear(COLOR_BLACK);
    PdqDisplayDrawText(10, 10, Line1, COLOR_GREEN);
    PdqDisplayDrawText(10, 30, Line2, COLOR_WHITE);
}
```

---

*This document is authoritative for all PDQminer code contributions.*
