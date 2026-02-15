# NerdMiner v2 SHA256 Mining Analysis

## Source: BitMaker-hub/NerdMiner_v2 (main branch)
## NMMiner (NMminer1024/NMMiner): CLOSED SOURCE - only firmware binaries

---

## 1. Performance Benchmarks (from nerdSHA_HWTest.cpp)

| Approach | ESP32-D0 (KH/s) | ESP32-S3 (KH/s) | Notes |
|----------|-----------------|-----------------|-------|
| Generic mbedtls software | ~9.5 | ~16 | Baseline |
| nerdSha256 (optimized SW) | ~39 | ~39 | IRAM, rotated A[], midstate |
| nerdSha256 bake | ~41 | ~42 | Pre-compute rounds 0-2 |
| Hardware high level (esp_sha_dma) | ~62 | — | HAL API with DMA |
| esp_sha with lock/unlock | ~5.5 | — | Lock overhead kills it |
| **ESP32-D0 HW SHA direct register** | **~200** | — | **Fastest on D0** |
| Hardware low level + midstate | ~156 | — | sha_hal_hash_block |
| Hardware LL (lowest level) | ~162 | — | Register-level |
| DMA hash | ~49.83 | — | lldesc_t linked list |
| **NMMiner claimed** | **~1,035** | **~398** | **Closed source** |

### PDQminer Current Architecture
- Software-only (PDQ_USE_HW_SHA = 0)
- Already has midstate + bake optimization (identical to NerdMiner)
- Uses `MINE_ROUND` macro (same as NerdMiner's `P()` macro)
- Early termination at round 60 (0x32E7 check)
- Deferred h computation in rounds 57-60
- Compiled with `-Os,no-unroll-loops` to avoid register spilling
- **Expected: ~40 KH/s per core software, ~80 KH/s dual-core**

---

## 2. Key Architectural Insights

### ESP32-D0 vs ESP32-S2/S3/C3 Register Layout
```
ESP32-D0 (original):
  - SHA_TEXT_BASE: Used for BOTH input text block AND digest output
  - SHA_256_BUSY_REG: Busy polling register
  - sha_ll_start_block() / sha_ll_continue_block() / sha_ll_load()
  - esp_sha_lock_engine() / esp_sha_unlock_engine()
  - Needs __builtin_bswap32() on digest read

ESP32-S2/S3/C3:
  - SHA_TEXT_BASE: Input text block
  - SHA_H_BASE: Separate digest state (read/write)
  - SHA_BUSY_REG: Busy polling register
  - SHA_START_REG / SHA_CONTINUE_REG
  - esp_sha_acquire_hardware() / esp_sha_release_hardware()
  - DPORT_INTERRUPT_DISABLE/RESTORE for thread safety
```

### Why NMMiner Achieves ~1035 KH/s (hypothesis)
The gap from NerdMiner's ~200 KH/s to NMMiner's ~1035 KH/s likely comes from:
1. **Dual-core parallel HW SHA**: Two cores each doing ~500 KH/s with HW SHA
2. **Per-core SHA engine locking**: Minimizing lock contention
3. **Aggressive assembly optimization**: Hand-tuned register access patterns
4. **Different measurement**: May count both cores combined vs single core

---

## 3. Hardware SHA256 Mining Flow (from mining.cpp)

### ESP32-S2/S3/C3 HW Mining Loop (minerWorkerHw)
```c
esp_sha_acquire_hardware();
REG_WRITE(SHA_MODE_REG, SHA2_256);

for (nonce = start; nonce < end; ++nonce) {
    // 1. Write midstate to digest registers
    nerd_sha_ll_write_digest(digest_mid);   // -> SHA_H_BASE
    
    // 2. Write block tail (with nonce) to text block
    nerd_sha_ll_fill_text_block_sha256(sha_buffer, nonce);  // -> SHA_TEXT_BASE
    
    // 3. Continue hash (uses existing digest state)
    REG_WRITE(SHA_CONTINUE_REG, 1);
    sha_ll_load(SHA2_256);
    nerd_sha_hal_wait_idle();
    
    // 4. Move first hash result to text block for second hash
    nerd_sha_ll_fill_text_block_sha256_inter();  // SHA_H_BASE -> SHA_TEXT_BASE + padding
    
    // 5. Start fresh hash (second SHA256)
    REG_WRITE(SHA_START_REG, 1);
    sha_ll_load(SHA2_256);
    nerd_sha_hal_wait_idle();
    
    // 6. Read digest with early rejection
    if (nerd_sha_ll_read_digest_if(hash)) {
        // Only reads full digest if upper 16 bits of word[7] are zero
        double diff = diff_from_target(hash);
        // ...submit if meets target
    }
}

esp_sha_release_hardware();
```

### ESP32-D0 HW Mining Loop (minerWorkerHw)
```c
esp_sha_lock_engine(SHA2_256);

for (nonce = 0; nonce < count; ++nonce) {
    // 1. Write entire first 64 bytes to SHA_TEXT_BASE (pre-swapped)
    nerd_sha_ll_fill_text_block_sha256(sha_buffer);
    sha_ll_start_block(SHA2_256);  // Start first block
    nerd_sha_hal_wait_idle();
    
    // 2. Write second 64 bytes (tail with nonce, padding)
    nerd_sha_ll_fill_text_block_sha256_upper(sha_buffer+64, nonce);
    sha_ll_continue_block(SHA2_256);  // Continue with second block
    nerd_sha_hal_wait_idle();
    sha_ll_load(SHA2_256);  // Load result
    
    // 3. Set up for second hash (double SHA)
    nerd_sha_hal_wait_idle();
    nerd_sha_ll_fill_text_block_sha256_double();  // padding only (digest already in TEXT_BASE)
    sha_ll_start_block(SHA2_256);  // Fresh start for second hash
    nerd_sha_hal_wait_idle();
    sha_ll_load(SHA2_256);
    
    // 4. Read with early rejection + byte swap
    if (nerd_sha_ll_read_digest_swap_if(hash)) {
        // ...check target
    }
}

esp_sha_unlock_engine(SHA2_256);
```

---

## 4. Critical Register Functions

### Fill Text Block (writes 16 uint32_t words to SHA peripheral input)
```c
// ESP32-S2/S3/C3
static inline void nerd_sha_ll_fill_text_block_sha256(const void *input_text, uint32_t nonce) {
    uint32_t *data_words = (uint32_t *)input_text;
    uint32_t *reg_addr_buf = (uint32_t *)(SHA_TEXT_BASE);
    REG_WRITE(&reg_addr_buf[0], data_words[0]);
    REG_WRITE(&reg_addr_buf[1], data_words[1]);
    REG_WRITE(&reg_addr_buf[2], data_words[2]);
    REG_WRITE(&reg_addr_buf[3], nonce);          // Nonce injected here
    REG_WRITE(&reg_addr_buf[4], 0x00000080);     // Padding
    // ... zeros ...
    REG_WRITE(&reg_addr_buf[15], 0x80020000);    // Length = 640 bits (BE)
}
```

### Write Digest State (load midstate into HW)
```c
static inline void nerd_sha_ll_write_digest(void *digest_state) {
    uint32_t *reg_addr_buf = (uint32_t *)(SHA_H_BASE);
    REG_WRITE(&reg_addr_buf[0], digest_state_words[0]);
    // ... 7 more words ...
}
```

### Read Digest with Early Rejection
```c
// ESP32-S2/S3: read from SHA_H_BASE, check word[7] upper 16 bits
static inline bool nerd_sha_ll_read_digest_if(void* ptr) {
    DPORT_INTERRUPT_DISABLE();
    uint32_t last = DPORT_SEQUENCE_REG_READ(SHA_H_BASE + 7 * 4);
    if ((uint16_t)(last >> 16) != 0) {
        DPORT_INTERRUPT_RESTORE();
        return false;  // Rejects ~99.998%
    }
    // Read remaining 7 words...
    DPORT_INTERRUPT_RESTORE();
    return true;
}

// ESP32-D0: read from SHA_TEXT_BASE with byte swap
static inline bool nerd_sha_ll_read_digest_swap_if(void* ptr) {
    DPORT_INTERRUPT_DISABLE();
    uint32_t fin = DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + 7 * 4);
    if ((fin & 0xFFFF) != 0) {
        DPORT_INTERRUPT_RESTORE();
        return false;
    }
    ((uint32_t*)ptr)[7] = __builtin_bswap32(fin);
    // Read remaining 7 words with bswap...
    DPORT_INTERRUPT_RESTORE();
    return true;
}
```

### Inter-hash Transfer (first hash result -> second hash input)
```c
// ESP32-S2/S3: Copy SHA_H_BASE -> SHA_TEXT_BASE for double-hash
static inline void nerd_sha_ll_fill_text_block_sha256_inter() {
    DPORT_INTERRUPT_DISABLE();
    REG_WRITE(&reg_addr_buf[0], DPORT_SEQUENCE_REG_READ(SHA_H_BASE + 0 * 4));
    // ... 7 more words (first hash output becomes second hash input)
    DPORT_INTERRUPT_RESTORE();
    REG_WRITE(&reg_addr_buf[8], 0x00000080);  // Padding for 32-byte message
    // ... zeros ...
    REG_WRITE(&reg_addr_buf[15], 0x00010000); // Length = 256 bits
}
```

---

## 5. NerdSHA256plus: The "Bake" Optimization

The `nerd_sha256_bake()` / `nerd_sha256d_baked()` pair is identical to PDQminer's 
`PdqBake()` / `PdqSha256dBaked()` architecture. Both pre-compute:
- W[0..2] (constant block tail words)
- W[16], W[17] (pre-expanded)
- State after rounds 0-2
- Partial round 3 T1 (without nonce) and T2

NerdMiner's `nerd_sha256d_baked()` also has late-stage optimization in the 
**second hash** (rounds 57-60):
- Computes only `d += t1` for rounds 57-60
- Checks `A[7] & 0xFFFF == 0x32E7` at round 60 for early rejection
- Only computes `h = t1 + t2` (deferred) if early check passes
- PDQminer already implements this exact same optimization

---

## 6. ESP-IDF Includes Required for HW SHA

```c
#include <sha/sha_dma.h>          // esp_sha_acquire/release_hardware
#include <hal/sha_hal.h>          // sha_hal_hash_block, sha_hal_read_digest
#include <hal/sha_ll.h>           // sha_ll_start_block, sha_ll_continue_block, sha_ll_load

// ESP32-D0 only:
#include <sha/sha_parallel_engine.h>  // esp_sha_lock_engine, esp_sha_unlock_engine
```

---

## 7. PDQminer Integration Roadmap

### Phase 1: Add HW SHA support (ESP32-S2/S3/C3)
- Implement `PdqHwSha256MineBlock()` using `nerd_sha_ll_*` register functions
- Separate midstate loading from text block writing
- Use `SHA_CONTINUE_REG` for midstate-resumed hashing
- Expected: ~62-162 KH/s per core

### Phase 2: Add HW SHA support (ESP32-D0)  
- Different register layout (SHA_TEXT_BASE for everything)
- Pre-swap byte order of block header
- Use `sha_ll_start_block` for first block, `sha_ll_continue_block` for second
- Expected: ~200 KH/s per core, ~400 KH/s dual-core

### Phase 3: Optimize HW/SW pipeline
- Run HW SHA on one core, SW SHA on the other
- Or HW SHA on both cores with proper locking
- Target: 400-500+ KH/s combined

---

## 8. File Manifest (NerdMiner_v2 reference files)

| File | Size | Purpose |
|------|------|---------|
| src/ShaTests/nerdSHA256.cpp | 13,428 | Software SHA256 (Transform, midstate, double_sha) |
| src/ShaTests/nerdSHA256.h | 1,231 | nerd_sha256 struct and function declarations |
| src/ShaTests/nerdSHA256plus.cpp | ~15K | Baked SHA256d (nerd_mids, nerd_sha256d, nerd_sha256_bake, nerd_sha256d_baked) |
| src/ShaTests/nerdSHA256plus.h | ~700 | nerdSHA256_context struct and declarations |
| src/ShaTests/nerdSHA_HWTest.cpp | 18,882 | ALL HW SHA benchmarks, register-level code |
| src/ShaTests/nerdSHA_HWTest.h | 1,624 | HwShaTest() declaration |
| src/mining.cpp | 44,997 | Full mining pipeline (stratum, job queues, HW/SW workers) |
