/**
 * @file sha256_engine.c
 * @brief High-performance SHA256 engine optimized for Bitcoin mining
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "sha256_engine.h"
#include <string.h>

/* Use -Os for the generic SHA256 engine AND SW mining functions: on Xtensa LX6
 * (16 registers with windows), -Os generates tighter code that fits better in
 * the instruction cache and register file. Benchmarks show -Os outperforms -O2
 * for the pure-ALU SHA256 round computation (~46 KH/s vs ~36 KH/s per core).
 * We use push/pop to restore -O2 only for HW SHA mining (register-mapped I/O). */
#pragma GCC push_options
#pragma GCC optimize("Os,no-unroll-loops")

#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define PDQ_USE_HW_SHA 0
#define PDQ_IRAM_ATTR IRAM_ATTR
#define PDQ_DRAM_ATTR DRAM_ATTR
#elif defined(ESP8266)
#include <Esp.h>
#define PDQ_USE_HW_SHA 0
#define PDQ_IRAM_ATTR ICACHE_RAM_ATTR
#define PDQ_DRAM_ATTR
#else
#define PDQ_USE_HW_SHA 0
#define PDQ_IRAM_ATTR
#define PDQ_DRAM_ATTR
#endif

static PDQ_DRAM_ATTR const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static PDQ_DRAM_ATTR const uint32_t H_INIT[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define MAJ(x, y, z) (((x) & (y)) | ((z) & ((x) ^ (y))))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

#define SHA256_ROUND(a, b, c, d, e, f, g, h, k, w) do { \
    register uint32_t t1 = (h) + EP1(e) + CH(e, f, g) + (k) + (w); \
    register uint32_t t2 = EP0(a) + MAJ(a, b, c); \
    (h) = (g); (g) = (f); (f) = (e); (e) = (d) + t1; \
    (d) = (c); (c) = (b); (b) = (a); (a) = t1 + t2; \
} while(0)

#define SHA256_ROUND_KW(a, b, c, d, e, f, g, h, kw) do { \
    register uint32_t t1 = (h) + EP1(e) + CH(e, f, g) + (kw); \
    register uint32_t t2 = EP0(a) + MAJ(a, b, c); \
    (h) = (g); (g) = (f); (f) = (e); (e) = (d) + t1; \
    (d) = (c); (c) = (b); (b) = (a); (a) = t1 + t2; \
} while(0)

/* Mining-optimized round macro with rotating A[] array positions.
 * Only updates d and h, eliminating 6 unnecessary variable shifts per round.
 * Based on Blockstream Jade / NerdMiner approach. */
#define MINE_ROUND(a, b, c, d, e, f, g, h, x, k) do { \
    uint32_t _t1 = (h) + EP1(e) + CH(e, f, g) + (k) + (x); \
    uint32_t _t2 = EP0(a) + MAJ(a, b, c); \
    (d) += _t1; \
    (h) = _t1 + _t2; \
} while(0)

/* Just-in-time W expansion macro - computes W[t] from prior W values */
#define MINE_W(W, t) ((W)[t] = SIG1((W)[(t) - 2]) + (W)[(t) - 7] + SIG0((W)[(t) - 15]) + (W)[(t) - 16])

static inline uint32_t ReadBe32(const uint8_t* p_Data) {
    return ((uint32_t)p_Data[0] << 24) | ((uint32_t)p_Data[1] << 16) |
           ((uint32_t)p_Data[2] << 8) | (uint32_t)p_Data[3];
}

static inline void WriteBe32(uint8_t* p_Data, uint32_t Value) {
    p_Data[0] = (uint8_t)(Value >> 24);
    p_Data[1] = (uint8_t)(Value >> 16);
    p_Data[2] = (uint8_t)(Value >> 8);
    p_Data[3] = (uint8_t)Value;
}

static inline void WriteLe32(uint8_t* p_Data, uint32_t Value) {
    p_Data[0] = (uint8_t)Value;
    p_Data[1] = (uint8_t)(Value >> 8);
    p_Data[2] = (uint8_t)(Value >> 16);
    p_Data[3] = (uint8_t)(Value >> 24);
}

static void Sha256Transform(uint32_t* p_State, const uint8_t* p_Block) {
    register uint32_t a, b, c, d, e, f, g, h;
    uint32_t W[64];

    for (int i = 0; i < 16; i++) {
        W[i] = ReadBe32(p_Block + i * 4);
    }

    for (int i = 16; i < 64; i++) {
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];
    }

    a = p_State[0]; b = p_State[1]; c = p_State[2]; d = p_State[3];
    e = p_State[4]; f = p_State[5]; g = p_State[6]; h = p_State[7];

    SHA256_ROUND(a,b,c,d,e,f,g,h, K[0], W[0]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[1], W[1]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[2], W[2]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[3], W[3]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[4], W[4]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[5], W[5]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[6], W[6]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[7], W[7]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[8], W[8]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[9], W[9]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[10], W[10]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[11], W[11]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[12], W[12]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[13], W[13]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[14], W[14]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[15], W[15]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[16], W[16]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[17], W[17]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[18], W[18]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[19], W[19]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[20], W[20]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[21], W[21]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[22], W[22]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[23], W[23]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[24], W[24]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[25], W[25]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[26], W[26]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[27], W[27]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[28], W[28]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[29], W[29]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[30], W[30]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[31], W[31]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[32], W[32]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[33], W[33]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[34], W[34]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[35], W[35]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[36], W[36]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[37], W[37]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[38], W[38]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[39], W[39]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[40], W[40]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[41], W[41]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[42], W[42]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[43], W[43]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[44], W[44]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[45], W[45]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[46], W[46]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[47], W[47]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[48], W[48]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[49], W[49]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[50], W[50]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[51], W[51]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[52], W[52]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[53], W[53]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[54], W[54]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[55], W[55]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[56], W[56]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[57], W[57]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[58], W[58]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[59], W[59]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[60], W[60]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[61], W[61]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[62], W[62]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[63], W[63]);

    p_State[0] += a; p_State[1] += b; p_State[2] += c; p_State[3] += d;
    p_State[4] += e; p_State[5] += f; p_State[6] += g; p_State[7] += h;
}

PdqError_t PdqSha256Init(PdqSha256Context_t* p_Ctx) {
    if (p_Ctx == NULL) return PdqErrorInvalidParam;
    memcpy(p_Ctx->State, H_INIT, sizeof(H_INIT));
    p_Ctx->ByteCount = 0;
    return PdqOk;
}

PdqError_t PdqSha256Update(PdqSha256Context_t* p_Ctx, const uint8_t* p_Data, size_t Length) {
    if (p_Ctx == NULL || (p_Data == NULL && Length > 0)) return PdqErrorInvalidParam;

    size_t BufferIdx = p_Ctx->ByteCount % 64;
    p_Ctx->ByteCount += Length;

    if (BufferIdx > 0) {
        size_t ToCopy = 64 - BufferIdx;
        if (ToCopy > Length) ToCopy = Length;
        memcpy(p_Ctx->Buffer + BufferIdx, p_Data, ToCopy);
        p_Data += ToCopy;
        Length -= ToCopy;
        BufferIdx += ToCopy;
        if (BufferIdx == 64) {
            Sha256Transform(p_Ctx->State, p_Ctx->Buffer);
        }
    }

    while (Length >= 64) {
        Sha256Transform(p_Ctx->State, p_Data);
        p_Data += 64;
        Length -= 64;
    }

    if (Length > 0) {
        memcpy(p_Ctx->Buffer, p_Data, Length);
    }

    return PdqOk;
}

PdqError_t PdqSha256Final(PdqSha256Context_t* p_Ctx, uint8_t* p_Hash) {
    if (p_Ctx == NULL || p_Hash == NULL) return PdqErrorInvalidParam;

    size_t BufferIdx = p_Ctx->ByteCount % 64;
    p_Ctx->Buffer[BufferIdx++] = 0x80;

    if (BufferIdx > 56) {
        memset(p_Ctx->Buffer + BufferIdx, 0, 64 - BufferIdx);
        Sha256Transform(p_Ctx->State, p_Ctx->Buffer);
        BufferIdx = 0;
    }

    memset(p_Ctx->Buffer + BufferIdx, 0, 56 - BufferIdx);
    uint64_t BitLen = (uint64_t)p_Ctx->ByteCount * 8;
    for (int i = 0; i < 8; i++) {
        p_Ctx->Buffer[63 - i] = (uint8_t)(BitLen >> (i * 8));
    }
    Sha256Transform(p_Ctx->State, p_Ctx->Buffer);

    for (int i = 0; i < 8; i++) {
        WriteBe32(p_Hash + i * 4, p_Ctx->State[i]);
    }

    return PdqOk;
}

PdqError_t PdqSha256(const uint8_t* p_Data, size_t Length, uint8_t* p_Hash) {
#if PDQ_USE_HW_SHA
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, p_Data, Length);
    mbedtls_sha256_finish(&ctx, p_Hash);
    mbedtls_sha256_free(&ctx);
    return PdqOk;
#else
    PdqSha256Context_t Ctx;
    PdqSha256Init(&Ctx);
    PdqSha256Update(&Ctx, p_Data, Length);
    return PdqSha256Final(&Ctx, p_Hash);
#endif
}

PdqError_t PdqSha256d(const uint8_t* p_Data, size_t Length, uint8_t* p_Hash) {
#if PDQ_USE_HW_SHA
    uint8_t FirstHash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, p_Data, Length);
    mbedtls_sha256_finish(&ctx, FirstHash);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, FirstHash, 32);
    mbedtls_sha256_finish(&ctx, p_Hash);
    mbedtls_sha256_free(&ctx);
    return PdqOk;
#else
    uint8_t FirstHash[32];
    PdqSha256(p_Data, Length, FirstHash);
    return PdqSha256(FirstHash, 32, p_Hash);
#endif
}

PdqError_t PdqSha256Midstate(const uint8_t* p_BlockHeader, uint8_t* p_Midstate) {
    if (p_BlockHeader == NULL || p_Midstate == NULL) return PdqErrorInvalidParam;

    uint32_t State[8];
    memcpy(State, H_INIT, sizeof(H_INIT));
    Sha256Transform(State, p_BlockHeader);

    for (int i = 0; i < 8; i++) {
        WriteBe32(p_Midstate + i * 4, State[i]);
    }

    return PdqOk;
}

PDQ_IRAM_ATTR static bool CheckTarget(const uint32_t* p_Hash, const uint32_t* p_Target) {
    /* Compare as LE uint256: pn[7] is most significant, pn[0] least.
     * Both Hash and Target use pn[] ordering: index 7 = most significant word. */
    for (int i = 7; i >= 0; i--) {
        if (p_Hash[i] > p_Target[i]) return false;
        if (p_Hash[i] < p_Target[i]) return true;
    }
    return true;
}

/* ============================================================================
 * Mining-optimized SHA256d - NerdMiner/Blockstream Jade architecture.
 *
 * Split into three functions to leverage Xtensa register windows:
 *   PdqBake()         - Pre-compute nonce-independent state (once per job batch)
 *   PdqSha256dBaked() - Per-nonce double SHA256 with baked state
 *   PdqSha256MineBlock() - Outer loop calling the above
 *
 * On ESP32's Xtensa LX6, each function call rotates the 16-register window,
 * giving the callee a fresh register context. This dramatically reduces
 * register spilling compared to a single monolithic function.
 * ============================================================================ */

/* Bake context: pre-computed nonce-independent values.
 * bake[0..2]  = W[0..2] (block tail words, constant per job)
 * bake[3]     = pre-computed W[16]
 * bake[4]     = pre-computed W[17]
 * bake[5..12] = SHA256 state A[0..7] after rounds 0-2
 * bake[13]    = partial round 3 T1 (without nonce-dependent W3)
 * bake[14]    = round 3 T2 */
#define BAKE_SIZE 15

PDQ_IRAM_ATTR __attribute__((noinline)) static void PdqBake(const uint32_t* p_Midstate, const uint8_t* p_BlockTail, uint32_t* p_Bake) {
    uint32_t temp1, temp2;

    p_Bake[0] = ReadBe32(p_BlockTail);
    p_Bake[1] = ReadBe32(p_BlockTail + 4);
    p_Bake[2] = ReadBe32(p_BlockTail + 8);

    /* Pre-compute W[16] = SIG1(0) + 0 + SIG0(W1) + W0
     * Pre-compute W[17] = SIG1(640) + 0 + SIG0(W2) + W1
     * (W[14]=0, W[15]=640=0x280, W[9]=0, W[10]=0) */
    p_Bake[3] = SIG1(0) + 0 + SIG0(p_Bake[1]) + p_Bake[0];
    p_Bake[4] = SIG1(640) + 0 + SIG0(p_Bake[2]) + p_Bake[1];

    /* Run rounds 0-2 with midstate as initial state */
    uint32_t* a = p_Bake + 5;
    a[0] = p_Midstate[0]; a[1] = p_Midstate[1];
    a[2] = p_Midstate[2]; a[3] = p_Midstate[3];
    a[4] = p_Midstate[4]; a[5] = p_Midstate[5];
    a[6] = p_Midstate[6]; a[7] = p_Midstate[7];

    MINE_ROUND(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7], p_Bake[0], K[0]);
    MINE_ROUND(a[7],a[0],a[1],a[2],a[3],a[4],a[5],a[6], p_Bake[1], K[1]);
    MINE_ROUND(a[6],a[7],a[0],a[1],a[2],a[3],a[4],a[5], p_Bake[2], K[2]);

    /* Partial round 3: pre-compute T1 base (without W3) and T2 */
    p_Bake[13] = a[4] + EP1(a[1]) + CH(a[1],a[2],a[3]) + K[3];
    p_Bake[14] = EP0(a[5]) + MAJ(a[5],a[6],a[7]);
}

PDQ_IRAM_ATTR __attribute__((noinline)) static bool PdqSha256dBaked(const uint32_t* p_Midstate, const uint8_t* p_BlockTail,
                                           const uint32_t* p_Bake, uint32_t* p_FinalState) {
    uint32_t temp1, temp2;

    /* === First hash: SHA256 of block tail with midstate === */
    uint32_t W[64];
    W[0] = p_Bake[0]; W[1] = p_Bake[1]; W[2] = p_Bake[2];
    W[3] = ReadBe32(p_BlockTail + 12);
    W[4] = 0x80000000;
    W[5] = 0; W[6] = 0; W[7] = 0; W[8] = 0;
    W[9] = 0; W[10] = 0; W[11] = 0; W[12] = 0;
    W[13] = 0; W[14] = 0;
    W[15] = 640;
    W[16] = p_Bake[3];
    W[17] = p_Bake[4];

    /* Load baked state (after rounds 0-2) */
    const uint32_t* ba = p_Bake + 5;
    uint32_t A[8] = { ba[0], ba[1], ba[2], ba[3],
                      ba[4], ba[5], ba[6], ba[7] };

    /* Complete round 3 with nonce-dependent W3 */
    temp1 = p_Bake[13] + W[3];
    temp2 = p_Bake[14];
    A[0] += temp1;
    A[4] = temp1 + temp2;

    /* Rounds 4-15 */
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], W[4], K[4]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], W[5], K[5]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], W[6], K[6]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], W[7], K[7]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], W[8], K[8]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], W[9], K[9]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], W[10], K[10]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], W[11], K[11]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], W[12], K[12]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], W[13], K[13]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], W[14], K[14]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], W[15], K[15]);

    /* Rounds 16-17 (pre-computed W) */
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], W[16], K[16]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], W[17], K[17]);

    /* Rounds 18-63 (just-in-time W expansion) */
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,18), K[18]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,19), K[19]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,20), K[20]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,21), K[21]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,22), K[22]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,23), K[23]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,24), K[24]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,25), K[25]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,26), K[26]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,27), K[27]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,28), K[28]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,29), K[29]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,30), K[30]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,31), K[31]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,32), K[32]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,33), K[33]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,34), K[34]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,35), K[35]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,36), K[36]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,37), K[37]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,38), K[38]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,39), K[39]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,40), K[40]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,41), K[41]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,42), K[42]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,43), K[43]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,44), K[44]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,45), K[45]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,46), K[46]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,47), K[47]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,48), K[48]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,49), K[49]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,50), K[50]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,51), K[51]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,52), K[52]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,53), K[53]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,54), K[54]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,55), K[55]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,56), K[56]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,57), K[57]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,58), K[58]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,59), K[59]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,60), K[60]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,61), K[61]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,62), K[62]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,63), K[63]);

    /* Finalize first hash */
    W[0] = p_Midstate[0] + A[0]; W[1] = p_Midstate[1] + A[1];
    W[2] = p_Midstate[2] + A[2]; W[3] = p_Midstate[3] + A[3];
    W[4] = p_Midstate[4] + A[4]; W[5] = p_Midstate[5] + A[5];
    W[6] = p_Midstate[6] + A[6]; W[7] = p_Midstate[7] + A[7];
    W[8] = 0x80000000; W[9] = 0; W[10] = 0; W[11] = 0;
    W[12] = 0; W[13] = 0; W[14] = 0; W[15] = 256;

    /* === Second hash: SHA256(intermediate_hash) === */
    A[0] = 0x6a09e667; A[1] = 0xbb67ae85; A[2] = 0x3c6ef372; A[3] = 0xa54ff53a;
    A[4] = 0x510e527f; A[5] = 0x9b05688c; A[6] = 0x1f83d9ab; A[7] = 0x5be0cd19;

    /* Second hash rounds 0-15 */
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], W[0], K[0]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], W[1], K[1]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], W[2], K[2]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], W[3], K[3]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], W[4], K[4]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], W[5], K[5]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], W[6], K[6]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], W[7], K[7]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], W[8], K[8]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], W[9], K[9]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], W[10], K[10]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], W[11], K[11]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], W[12], K[12]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], W[13], K[13]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], W[14], K[14]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], W[15], K[15]);

    /* Second hash rounds 16-56 */
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,16), K[16]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,17), K[17]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,18), K[18]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,19), K[19]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,20), K[20]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,21), K[21]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,22), K[22]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,23), K[23]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,24), K[24]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,25), K[25]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,26), K[26]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,27), K[27]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,28), K[28]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,29), K[29]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,30), K[30]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,31), K[31]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,32), K[32]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,33), K[33]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,34), K[34]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,35), K[35]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,36), K[36]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,37), K[37]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,38), K[38]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,39), K[39]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,40), K[40]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,41), K[41]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,42), K[42]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,43), K[43]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,44), K[44]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,45), K[45]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,46), K[46]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,47), K[47]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,48), K[48]);
    MINE_ROUND(A[7],A[0],A[1],A[2],A[3],A[4],A[5],A[6], MINE_W(W,49), K[49]);
    MINE_ROUND(A[6],A[7],A[0],A[1],A[2],A[3],A[4],A[5], MINE_W(W,50), K[50]);
    MINE_ROUND(A[5],A[6],A[7],A[0],A[1],A[2],A[3],A[4], MINE_W(W,51), K[51]);
    MINE_ROUND(A[4],A[5],A[6],A[7],A[0],A[1],A[2],A[3], MINE_W(W,52), K[52]);
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,53), K[53]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,54), K[54]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,55), K[55]);
    MINE_ROUND(A[0],A[1],A[2],A[3],A[4],A[5],A[6],A[7], MINE_W(W,56), K[56]);

    /* Deferred rounds 57-60: only compute d += t1, defer h = t1 + t2.
     * This enables early termination before computing rounds 61-63. */

    /* Round 57 */
    uint32_t m1 = A[6] + EP1(A[3]) + CH(A[3],A[4],A[5]) + K[57] + MINE_W(W,57);
    A[2] += m1;
    uint32_t d57_a1 = A[1];

    /* Round 58 */
    uint32_t z1 = A[5] + EP1(A[2]) + CH(A[2],A[3],A[4]) + K[58] + MINE_W(W,58);
    uint32_t d58_a0 = A[0];
    A[1] += z1;

    /* Round 59 */
    uint32_t y1 = A[4] + EP1(A[1]) + CH(A[1],A[2],A[3]) + K[59] + MINE_W(W,59);
    A[0] += y1;

    /* Round 60 */
    uint32_t x1 = A[3] + EP1(A[0]) + CH(A[0],A[1],A[2]) + K[60] + MINE_W(W,60);
    uint32_t a7 = A[7] + x1;

    /* Early termination: for a valid hash, hash[7] = 0x5be0cd19 + A[7].
     * Lower 16 bits of A[7] must be 0x32E7. Rejects ~99.998%. */
    if ((a7 & 0xFFFF) != 0x32E7)
        return false;

    /* Post-compute deferred h values for rounds 57-60 */
    { uint32_t m2 = EP0(A[7]) + MAJ(A[7], d58_a0, d57_a1); A[6] = m1 + m2; }
    { uint32_t z2 = EP0(A[6]) + MAJ(A[6], A[7], d58_a0); A[5] = z1 + z2; }
    { uint32_t y2 = EP0(A[5]) + MAJ(A[5], A[6], A[7]); A[4] = y1 + y2; }
    A[7] = a7;
    { uint32_t x2 = EP0(A[4]) + MAJ(A[4], A[5], A[6]); A[3] = x1 + x2; }

    /* Rounds 61-63 */
    MINE_ROUND(A[3],A[4],A[5],A[6],A[7],A[0],A[1],A[2], MINE_W(W,61), K[61]);
    MINE_ROUND(A[2],A[3],A[4],A[5],A[6],A[7],A[0],A[1], MINE_W(W,62), K[62]);
    MINE_ROUND(A[1],A[2],A[3],A[4],A[5],A[6],A[7],A[0], MINE_W(W,63), K[63]);

    /* Compute final state */
    p_FinalState[0] = 0x6a09e667 + A[0];
    p_FinalState[1] = 0xbb67ae85 + A[1];
    p_FinalState[2] = 0x3c6ef372 + A[2];
    p_FinalState[3] = 0xa54ff53a + A[3];
    p_FinalState[4] = 0x510e527f + A[4];
    p_FinalState[5] = 0x9b05688c + A[5];
    p_FinalState[6] = 0x1f83d9ab + A[6];
    p_FinalState[7] = 0x5be0cd19 + A[7];

    return true;
}

PDQ_IRAM_ATTR PdqError_t PdqSha256MineBlock(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    if (p_Job == NULL || p_Nonce == NULL || p_Found == NULL) return PdqErrorInvalidParam;

    *p_Found = false;

    /* Read midstate as uint32_t (once per batch) */
    uint32_t MidState[8];
    for (int i = 0; i < 8; i++) {
        MidState[i] = ReadBe32(p_Job->Midstate + i * 4);
    }

    /* Bake nonce-independent state (once per batch) */
    uint32_t Bake[BAKE_SIZE];
    PdqBake(MidState, p_Job->BlockTail, Bake);

    /* Prepare block tail with nonce position for per-nonce function.
     * BlockTail bytes 12-15 contain the nonce in big-endian. */
    uint8_t BlockTail[16];
    memcpy(BlockTail, p_Job->BlockTail, 16);

    uint32_t TargetHigh = p_Job->Target[7];

    uint32_t Nonce = p_Job->NonceStart;
    for (;;) {
        /* Write nonce into block tail (little-endian, matching block header format) */
        BlockTail[12] = (uint8_t)(Nonce);
        BlockTail[13] = (uint8_t)(Nonce >> 8);
        BlockTail[14] = (uint8_t)(Nonce >> 16);
        BlockTail[15] = (uint8_t)(Nonce >> 24);

        /* Run double SHA256 with baked pre-computation */
        uint32_t FinalState[8];
        if (PdqSha256dBaked(MidState, BlockTail, Bake, FinalState)) {
            /* Passed early termination - do full target check */
            if (FinalState[7] <= TargetHigh) {
                if (CheckTarget(FinalState, p_Job->Target)) {
                    *p_Nonce = Nonce;
                    *p_Found = true;
                    return PdqOk;
                }
            }
        }

        if (Nonce == p_Job->NonceEnd) break;
        Nonce++;
    }

    return PdqOk;
}

/* ============================================================================
 * Hardware SHA256 mining for ESP32-D0 (Xtensa LX6)
 *
 * Uses the SHA256 hardware peripheral at 0x3FF03000 for massive speedup.
 * The HW engine processes a 64-byte block in ~83 CPU cycles at 240MHz.
 *
 * Architecture notes (ESP32-D0):
 *   - Data is written to SHA_TEXT_BASE (0x3FF03000), 16 x 32-bit words
 *   - Digest is read from SHA_TEXT_BASE after LOAD (not SHA_H_BASE like S2/S3)
 *   - LOAD copies internal state → SHA_TEXT[0:7] (read-only, no state restore)
 *   - Midstate caching is NOT possible (ESP32-D0 lacks SOC_SHA_SUPPORT_RESUME)
 *   - Writes to SHA_TEXT during START are SAFE (engine latches data at trigger)
 *   - Writes to SHA_TEXT during CONTINUE are UNSAFE (engine reads progressively)
 *
 * Optimizations applied:
 *   1. Overlap block1 fill with block0 START (hide 16 APB writes behind SHA)
 *   2. Overlap block0 fill with 2nd hash START (hide 16 APB writes behind SHA)
 *   3. Reduced block0 refill: only 8 words ([8:15] preserved from overlap fill)
 *   4. Reduced double-hash padding: only 2 words differ from block1[8:15]
 *   5. Compiled at -O2 (restored from -Os push/pop for HW register I/O)
 * ============================================================================ */

/* Restore -O2 for HW SHA mining code (register-mapped I/O, no complex ALU). */
#pragma GCC pop_options

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32)

#include "soc/dport_reg.h"
#include "soc/hwcrypto_reg.h"
#include "sha/sha_parallel_engine.h"
#include "hal/sha_ll.h"

/* Write 16 big-endian words to SHA_TEXT_BASE registers.
 * Non-volatile pointer matches NerdMiner; all writes are ordered by dependency.
 * always_inline ensures this stays in IRAM with the caller. */
static inline __attribute__((always_inline)) void HwShaFillBlock(const uint32_t* p_Data) {
    uint32_t* Reg = (uint32_t*)SHA_TEXT_BASE;
    Reg[0]  = p_Data[0];  Reg[1]  = p_Data[1];
    Reg[2]  = p_Data[2];  Reg[3]  = p_Data[3];
    Reg[4]  = p_Data[4];  Reg[5]  = p_Data[5];
    Reg[6]  = p_Data[6];  Reg[7]  = p_Data[7];
    Reg[8]  = p_Data[8];  Reg[9]  = p_Data[9];
    Reg[10] = p_Data[10]; Reg[11] = p_Data[11];
    Reg[12] = p_Data[12]; Reg[13] = p_Data[13];
    Reg[14] = p_Data[14]; Reg[15] = p_Data[15];
}

/* Write the second block (bytes 64-127) with a specific nonce at word 3 */
static inline __attribute__((always_inline)) void HwShaFillUpperBlock(const uint32_t* p_Data, uint32_t NonceSwapped) {
    uint32_t* Reg = (uint32_t*)SHA_TEXT_BASE;
    Reg[0]  = p_Data[0];
    Reg[1]  = p_Data[1];
    Reg[2]  = p_Data[2];
    Reg[3]  = NonceSwapped;        /* nonce goes at byte offset 76 = word 19 of header = word 3 of block 2 */
    Reg[4]  = 0x80000000;          /* SHA padding: 0x80 byte */
    Reg[5]  = 0x00000000;
    Reg[6]  = 0x00000000;
    Reg[7]  = 0x00000000;
    Reg[8]  = 0x00000000;
    Reg[9]  = 0x00000000;
    Reg[10] = 0x00000000;
    Reg[11] = 0x00000000;
    Reg[12] = 0x00000000;
    Reg[13] = 0x00000000;
    Reg[14] = 0x00000000;
    Reg[15] = 0x00000280;          /* length = 640 bits */
}

/* Write padding for the second SHA256 pass (hash of 32-byte intermediate) */
static inline __attribute__((always_inline)) void HwShaFillDoubleBlock(void) {
    uint32_t* Reg = (uint32_t*)SHA_TEXT_BASE;
    /* Words 0-7 already contain the intermediate hash (from sha_ll_load) */
    Reg[8]  = 0x80000000;          /* SHA padding: 0x80 byte */
    Reg[9]  = 0x00000000;
    Reg[10] = 0x00000000;
    Reg[11] = 0x00000000;
    Reg[12] = 0x00000000;
    Reg[13] = 0x00000000;
    Reg[14] = 0x00000000;
    Reg[15] = 0x00000100;          /* length = 256 bits */
}

/* Wait for HW SHA engine to finish.
 * Direct volatile read bypasses the expensive DPORT_REG_READ workaround
 * (which does interrupt disable + APB pre-read per call).
 * Safe because we hold the SHA engine lock and only poll from one core. */
static inline __attribute__((always_inline)) void HwShaWaitIdle(void) {
    while (*(volatile uint32_t*)SHA_256_BUSY_REG) {}
}

/* Read digest from SHA_TEXT_BASE with early exit check on word[7].
 * On ESP32-D0, digest is at SHA_TEXT_BASE (not SHA_H_BASE).
 * Returns false if lower 16 bits of last hash word are non-zero.
 * Uses direct volatile reads - safe with SHA engine lock held. */
static inline __attribute__((always_inline)) bool HwShaReadDigestSwapIf(uint32_t* p_Hash) {
    volatile uint32_t* Reg = (volatile uint32_t*)SHA_TEXT_BASE;
    uint32_t Last = Reg[7];
    if ((Last & 0xFFFF) != 0) {
        return false;
    }
    p_Hash[7] = __builtin_bswap32(Last);
    p_Hash[0] = __builtin_bswap32(Reg[0]);
    p_Hash[1] = __builtin_bswap32(Reg[1]);
    p_Hash[2] = __builtin_bswap32(Reg[2]);
    p_Hash[3] = __builtin_bswap32(Reg[3]);
    p_Hash[4] = __builtin_bswap32(Reg[4]);
    p_Hash[5] = __builtin_bswap32(Reg[5]);
    p_Hash[6] = __builtin_bswap32(Reg[6]);
    return true;
}

/* Write first 8 words of block0 to SHA_TEXT[0:7].
 * Used in the hot loop where SHA_TEXT[8:15] already has block0[8:15]
 * from the previous iteration's overlapped fill. */
static inline __attribute__((always_inline)) void HwShaFillBlock0Half(const uint32_t* p_Data) {
    uint32_t* Reg = (uint32_t*)SHA_TEXT_BASE;
    Reg[0]  = p_Data[0];  Reg[1]  = p_Data[1];
    Reg[2]  = p_Data[2];  Reg[3]  = p_Data[3];
    Reg[4]  = p_Data[4];  Reg[5]  = p_Data[5];
    Reg[6]  = p_Data[6];  Reg[7]  = p_Data[7];
}

/* Write last 8 words of block0 to SHA_TEXT[8:15].
 * Used during double-hash START overlap: only [8:15] needs filling because
 * the subsequent LOAD overwrites [0:7] with the final hash anyway. */
static inline __attribute__((always_inline)) void HwShaFillBlock0Upper(const uint32_t* p_Data) {
    uint32_t* Reg = (uint32_t*)SHA_TEXT_BASE;
    Reg[8]  = p_Data[8];  Reg[9]  = p_Data[9];
    Reg[10] = p_Data[10]; Reg[11] = p_Data[11];
    Reg[12] = p_Data[12]; Reg[13] = p_Data[13];
    Reg[14] = p_Data[14]; Reg[15] = p_Data[15];
}

/* Manual byte-swap: avoids __bswapsi2 library call on Xtensa (no HW bswap instr).
 * GCC's __builtin_bswap32 emits a callx8 to libgcc on this target even at -O2,
 * adding ~10 cycles of window-rotation overhead per call in the hot loop. */
static inline __attribute__((always_inline)) uint32_t Bswap32(uint32_t v) {
    v = ((v << 8) & 0xFF00FF00u) | ((v >> 8) & 0x00FF00FFu);
    return (v << 16) | (v >> 16);
}

/* Cold path hash candidate check — extracted as noinline to isolate register
 * pressure from the hot mining loop.  The 8× Bswap32 calls need many scratch
 * registers; keeping them in a separate function lets GCC's register allocator
 * use more registers for block0 caching in the hot loop.
 * Returns true if hash meets target (share found). */
static __attribute__((noinline)) bool HwCheckHashCandidate(
    uint32_t Nonce, const PdqMiningJob_t* p_Job)
{
    volatile uint32_t* vbase = (volatile uint32_t*)SHA_TEXT_BASE;
    uint32_t FinalHash[8];
    FinalHash[7] = Bswap32((uint32_t)vbase[7]);
    FinalHash[0] = Bswap32((uint32_t)vbase[0]);
    FinalHash[1] = Bswap32((uint32_t)vbase[1]);
    FinalHash[2] = Bswap32((uint32_t)vbase[2]);
    FinalHash[3] = Bswap32((uint32_t)vbase[3]);
    FinalHash[4] = Bswap32((uint32_t)vbase[4]);
    FinalHash[5] = Bswap32((uint32_t)vbase[5]);
    FinalHash[6] = Bswap32((uint32_t)vbase[6]);
    if (FinalHash[7] <= p_Job->Target[7] &&
        CheckTarget(FinalHash, p_Job->Target)) {
        return true;
    }
    return false;
}

/* =====================================================================
 * NOP TIMING MACROS for HW SHA pipeline.
 * NOP macro: single asm block prevents compiler from inserting instructions
 * between NOPs or reordering them.
 * ===================================================================== */

/* Phase 1: after block1 fill (16 stores), before CONTINUE.
 * Conservative after removing Bswap from hot loop (was 2 with Bswap).
 * Will be re-tuned by NOP binary search. */
#define NOP_13 __asm__ volatile( \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n" \
    ::: "memory")

/* Phase 4: between LOAD and START (double-hash).
 * 9 NOPs = empirical minimum (NOP_8=FAIL). */
#define NOP_9 __asm__ volatile( \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n" \
    ::: "memory")

/* Phase 6: after final LOAD, before l16ui hash check.
 * Original value: 15 NOPs. */
#define NOP_15 __asm__ volatile( \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n" \
    ::: "memory")

/* Phase 8: after START (block0 next iter), before bound check/nonce++.
 * Original value: 8 NOPs. */
#define NOP_8 __asm__ volatile( \
    "nop.n" \
    ::: "memory")

/* Phase 3: between CONTINUE and LOAD.
 * Original value: 57 NOPs. */
#define NOP_57 __asm__ volatile( \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n" \
    ::: "memory")

/* Phase 5: between START (double-hash) and LOAD (final result).
 * Original value: 50 NOPs. */
#define NOP_50 __asm__ volatile( \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\nnop.n\n" \
    "nop.n\nnop.n" \
    ::: "memory")

/* ============================================================================
 * HW SHA correctness test: computes SHA256d of a known 80-byte header using
 * the HW peripheral (same register sequence as the mining loop) and compares
 * with the SW result.  Uses busy-wait (not NOPs) for guaranteed correctness.
 * Returns true if HW matches SW.
 * ============================================================================ */
bool PdqSha256HwCorrectnessTest(void) {
    extern int printf(const char*, ...);

    /* Bitcoin block #1 header (80 bytes, known good) */
    static const uint8_t TestHeader[80] = {
        0x01,0x00,0x00,0x00,  /* version */
        0x6f,0xe2,0x8c,0x0a,0xb6,0xf1,0xb3,0x72,0xc1,0xa6,0xa2,0x46,
        0xae,0x63,0xf7,0x4f,0x93,0x1e,0x83,0x65,0xe1,0x5a,0x08,0x9c,
        0x68,0xd6,0x19,0x00,0x00,0x00,0x00,0x00,  /* prevhash */
        0x98,0x20,0x51,0xfd,0x1e,0x4b,0xa7,0x44,0xbb,0xbe,0x68,0x0e,
        0x1f,0xee,0x14,0x67,0x7b,0xa1,0xa3,0xc3,0x54,0x0b,0xf7,0xb1,
        0xcd,0xb6,0x06,0xe8,0x57,0x23,0x3e,0x0e,  /* merkle root */
        0x61,0xbc,0x66,0x49,  /* ntime */
        0xff,0xff,0x00,0x1d,  /* nbits */
        0x01,0xe3,0x62,0x99   /* nonce (block #1 actual nonce) */
    };

    /* SW path */
    uint8_t SwHash[32];
    PdqSha256d(TestHeader, 80, SwHash);

    printf("[TEST] SW SHA256d: ");
    for (int i = 31; i >= 0; i--) printf("%02x", SwHash[i]);
    printf("\n");

    /* HW path: same register sequence as mining loop */
    esp_sha_lock_engine(SHA2_256);

    volatile uint32_t* const BusyReg = (volatile uint32_t*)SHA_256_BUSY_REG;
    uint32_t* TextNV = (uint32_t*)SHA_TEXT_BASE;
    volatile uint32_t* TextV = (volatile uint32_t*)SHA_TEXT_BASE;

    /* Build block0 (first 64 bytes) and block1 (next 64 bytes with padding)
     * as big-endian uint32 words, same as HeaderSwapped. */
    uint32_t Block0[16], Block1[16];
    const uint32_t* HdrWords = (const uint32_t*)TestHeader;
    for (int i = 0; i < 16; i++) Block0[i] = __builtin_bswap32(HdrWords[i]);
    for (int i = 0; i < 4; i++)  Block1[i] = __builtin_bswap32(HdrWords[16 + i]);
    Block1[4] = 0x80000000;  /* SHA padding bit */
    for (int i = 5; i < 15; i++) Block1[i] = 0;
    Block1[15] = 0x00000280; /* bit length = 640 */

    /* Phase 1: Write block0, START */
    for (int i = 0; i < 16; i++) TextNV[i] = Block0[i];
    __asm__ volatile("memw" ::: "memory");
    TextV[36] = 1;  /* SHA256_START */
    while (*BusyReg) {}

    /* Phase 2: Write block1, CONTINUE */
    for (int i = 0; i < 16; i++) TextNV[i] = Block1[i];
    __asm__ volatile("memw" ::: "memory");
    TextV[37] = 1;  /* SHA256_CONTINUE */
    while (*BusyReg) {}

    /* Phase 3: LOAD intermediate hash */
    TextV[38] = 1;  /* SHA256_LOAD */
    __asm__ volatile("memw" ::: "memory");

    /* Now SHA_TEXT[0:7] = first SHA256 hash. Set up double-hash block. */
    TextNV[8]  = 0x80000000;
    TextNV[9]  = 0;
    TextNV[10] = 0;
    TextNV[11] = 0;
    TextNV[12] = 0;
    TextNV[13] = 0;
    TextNV[14] = 0;
    TextNV[15] = 0x00000100;  /* bit length = 256 */
    __asm__ volatile("memw" ::: "memory");

    /* Phase 4: START double-hash */
    TextV[36] = 1;  /* SHA256_START */
    while (*BusyReg) {}

    /* Phase 5: LOAD final hash */
    TextV[38] = 1;  /* SHA256_LOAD */
    __asm__ volatile("memw" ::: "memory");

    /* Read final hash from SHA_TEXT[0:7] (big-endian words → LE bytes) */
    uint8_t HwHash[32];
    for (int i = 0; i < 8; i++) {
        uint32_t w = __builtin_bswap32((uint32_t)TextV[i]);
        HwHash[i*4+0] = (uint8_t)(w);
        HwHash[i*4+1] = (uint8_t)(w >> 8);
        HwHash[i*4+2] = (uint8_t)(w >> 16);
        HwHash[i*4+3] = (uint8_t)(w >> 24);
    }

    esp_sha_unlock_engine(SHA2_256);

    printf("[TEST] HW SHA256d: ");
    for (int i = 31; i >= 0; i--) printf("%02x", HwHash[i]);
    printf("\n");

    bool Match = (memcmp(SwHash, HwHash, 32) == 0);
    printf("[TEST] HW vs SW (busy-wait): %s\n", Match ? "MATCH" : "MISMATCH");

    /* === Test 2: NOP-timed pipeline (same sequence as mining loop) === */
    esp_sha_lock_engine(SHA2_256);

    uint32_t* base = (uint32_t*)SHA_TEXT_BASE;
    __asm__ volatile("" : "+r"(base));
    uint32_t trigger = 1;

    /* Initial block0 fill + START */
    for (int i = 0; i < 16; i++) base[i] = Block0[i];
    __asm__ volatile("memw" ::: "memory");
    base[36] = trigger;  /* SHA256_START */
    __asm__ volatile("memw" ::: "memory");

    /* Fill block1 into SHA_TEXT (16 stores ~32 cycles) */
    base[0]  = Block1[0];  base[1]  = Block1[1];
    base[2]  = Block1[2];  base[3]  = Block1[3];
    base[4]  = Block1[4];  base[5]  = Block1[5];
    base[6]  = Block1[6];  base[7]  = Block1[7];
    base[8]  = Block1[8];  base[9]  = Block1[9];
    base[10] = Block1[10]; base[11] = Block1[11];
    base[12] = Block1[12]; base[13] = Block1[13];
    base[14] = Block1[14]; base[15] = Block1[15];

    NOP_13;
    __asm__ volatile("memw" ::: "memory");
    base[37] = trigger;  /* SHA256_CONTINUE */
    NOP_57;

    /* Double-hash padding */
    base[8]  = 0x80000000;
    base[15] = 0x00000100;
    __asm__ volatile("memw" ::: "memory");

    base[38] = trigger;  /* SHA256_LOAD */
    NOP_9;
    __asm__ volatile("memw" ::: "memory");
    base[36] = trigger;  /* SHA256_START (double-hash) */
    NOP_50;

    /* Restore block0[8:15] (same as mining loop Phase 5) */
    base[8]  = Block0[8];  base[9]  = Block0[9];
    base[10] = Block0[10]; base[11] = Block0[11];
    base[12] = Block0[12]; base[13] = Block0[13];
    base[14] = Block0[14]; base[15] = Block0[15];
    __asm__ volatile("memw" ::: "memory");

    base[38] = trigger;  /* SHA256_LOAD (final hash) */
    NOP_15;

    /* Read final hash */
    uint8_t NopHash[32];
    volatile uint32_t* vb = (volatile uint32_t*)SHA_TEXT_BASE;
    for (int i = 0; i < 8; i++) {
        uint32_t w = Bswap32((uint32_t)vb[i]);
        NopHash[i*4+0] = (uint8_t)(w);
        NopHash[i*4+1] = (uint8_t)(w >> 8);
        NopHash[i*4+2] = (uint8_t)(w >> 16);
        NopHash[i*4+3] = (uint8_t)(w >> 24);
    }

    esp_sha_unlock_engine(SHA2_256);

    printf("[TEST] NOP-timed:  ");
    for (int i = 31; i >= 0; i--) printf("%02x", NopHash[i]);
    printf("\n");

    bool NopMatch = (memcmp(SwHash, NopHash, 32) == 0);
    printf("[TEST] NOP vs SW: %s\n", NopMatch ? "MATCH" : "MISMATCH");

    return Match && NopMatch;
}

PdqError_t PdqSha256MineBlockHw(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    if (p_Job == NULL || p_Nonce == NULL || p_Found == NULL) return PdqErrorInvalidParam;
    *p_Found = false;

    /* HeaderSwapped: 80-byte header as 20 big-endian words + SHA padding to 32 words.
     * Block0 = words 0-15 (first 64B), Block1 = words 16-31 (next 64B with padding). */
    const uint32_t* Block0 = p_Job->HeaderSwapped;
    const uint32_t* Block1 = p_Job->HeaderSwapped + 16;

    /* Block1 loaded directly from p_Job each iteration (no stack copy).
     * Eliminates Block1Buf[16] + memcpy, freeing one register (was a6). */

    esp_sha_lock_engine(SHA2_256);

    /* Single base pointer for ALL SHA register access (base+offset addressing).
     * SHA_TEXT[0:15] at byte offsets 0-60.
     * SHA256_START = base[36]   (byte offset 144 = 0x90)
     * SHA256_CONTINUE = base[37] (byte offset 148 = 0x94)
     * SHA256_LOAD = base[38]    (byte offset 152 = 0x98)
     *
     * CRITICAL: The __asm__ volatile barrier prevents GCC from constant-folding
     * (uint32_t*)SHA_TEXT_BASE. Without it, GCC resolves each base[N] to a
     * separate l32r literal-pool entry (30+ extra l32r instructions per nonce).
     * With it, GCC uses s32i/s32i.n with offset from a SINGLE base register,
     * matching NMMiner's Rev3 codegen pattern. */
    uint32_t* base = (uint32_t*)SHA_TEXT_BASE;
    __asm__ volatile("" : "+r"(base));

    /* Trigger value kept in a register: avoids movi.n per trigger write.
     * NMMiner uses a15=1 throughout entire loop for START/CONTINUE/LOAD. */
    uint32_t trigger = 1;

    /* Cache block0[0:5] in local variables (compiler keeps in Xtensa registers).
     * NMMiner caches 6 words in a9-a14; block0[6:7] loaded from memory. */
    const uint32_t b0_0 = Block0[0], b0_1 = Block0[1], b0_2 = Block0[2];
    const uint32_t b0_3 = Block0[3], b0_4 = Block0[4], b0_5 = Block0[5];

    uint32_t Nonce = p_Job->NonceStart;
    uint32_t NonceEnd = p_Job->NonceEnd;

    /* === Initial block0: full 16-word fill + START ===
     * First time only: SHA_TEXT is cold, need all 16 words. */
    HwShaFillBlock(Block0);
    __asm__ volatile("memw" ::: "memory");
    base[36] = trigger;  /* SHA256_START */
    __asm__ volatile("" ::: "memory");

    /* =====================================================================
     * NOP-TIMED HOT LOOP (matching NMMiner Rev3 pipeline strategy)
     *
     * The ESP32 SHA peripheral latches SHA_TEXT data immediately on trigger.
     * LOAD acts as an internal sync point (waits for computation to complete).
     * NOP timing avoids the overhead of BUSY polling (volatile load + branch
     * + pipeline flush per poll iteration ≈ 4-6 cycles wasted per wait).
     *
     * 5 SHA triggers per nonce:
     *   CONTINUE(block1) → LOAD(intermediate) → START(double-hash) →
     *   LOAD(final) → START(block0 for next nonce)
     *
     * NOP counts empirically optimized via binary search.
     * Each NOP was reduced until bogus shares appeared, then backed off to safe value.
     * ===================================================================== */

    for (;;) {
        /* --- Phase 1: Fill block1[0:15] to SHA_TEXT while block0 START runs ---
         * Block1 loaded directly from p_Job (no stack copy buffer).
         * Nonce word (Block1[3]) replaced by Bswap32(Nonce).
         * Engine already latched block0 at START trigger; safe to overwrite SHA_TEXT.
         * NMMiner: 32 instructions (16× load+store pairs) + 13 NOPs before CONTINUE. */
        base[0]  = Block1[0];  base[1]  = Block1[1];
        base[2]  = Block1[2];  base[3]  = Bswap32(Nonce);
        base[4]  = Block1[4];  base[5]  = Block1[5];
        base[6]  = Block1[6];  base[7]  = Block1[7];
        base[8]  = Block1[8];  base[9]  = Block1[9];
        base[10] = Block1[10]; base[11] = Block1[11];
        base[12] = Block1[12]; base[13] = Block1[13];
        base[14] = Block1[14]; base[15] = Block1[15];

        /* NMMiner: 13 NOPs after block1 fill, before CONTINUE trigger.
         * Ensures block0 START has had enough cycles to latch and begin. */
        NOP_13;

        /* --- Phase 2: Trigger CONTINUE for block1 --- */
        __asm__ volatile("memw" ::: "memory");
        base[37] = trigger;  /* SHA256_CONTINUE */

        /* --- Phase 3: NOP wait for CONTINUE, write dbl-hash padding, LOAD ---
         * NMMiner: 59 NOPs, then write padding (0x80000000 and 0x100), 1 NOP, LOAD.
         * Our version: 57 NOPs, padding writes, LOAD.
         * CONTINUE→LOAD total budget: ~63 cycles (matching NMMiner's 64). */
        NOP_57;

        /* Pre-write double-hash padding before LOAD.
         * After LOAD, SHA_TEXT[0:7] = intermediate hash, [8:15] untouched.
         * Set [8]=0x80000000 (SHA padding) and [15]=0x100 (bit length=256). */
        base[8]  = 0x80000000;
        base[15] = 0x00000100;
        __asm__ volatile("memw" ::: "memory");

        base[38] = trigger;  /* SHA256_LOAD: intermediate hash → SHA_TEXT[0:7] */

        /* --- Phase 4: LOAD→START gap.  NMMiner uses 9 NOPs.
         * LOAD is fast (~8 cycles); need result in SHA_TEXT before START reads it.
         * SHA_TEXT now: [H0..H7, 0x80000000, 0, 0, 0, 0, 0, 0, 0x100] */
        NOP_9;
        __asm__ volatile("" ::: "memory");  /* no memw: LOAD trigger 9+ cycles ago, write buffer drained */

        base[36] = trigger;  /* SHA256_START (double-hash) */

        /* --- Phase 5: Restore block0[8:15] while double-hash runs + NOP wait ---
         * SAFE to write SHA_TEXT during START (engine latches data at trigger).
         * LOAD will overwrite only [0:7], preserving [8:15] = block0[8:15].
         * 50 NOPs + 17 instructions = 67 cycles total, known-good timing. */
        NOP_50;
        base[8]  = Block0[8];  base[9]  = Block0[9];
        base[10] = Block0[10]; base[11] = Block0[11];
        base[12] = Block0[12]; base[13] = Block0[13];
        base[14] = Block0[14]; base[15] = Block0[15];

        /* --- Phase 6: LOAD final result --- */
        __asm__ volatile("memw" ::: "memory");
        base[38] = trigger;  /* SHA256_LOAD: final hash → SHA_TEXT[0:7] */

        /* --- Phase 7: Wait for final LOAD + check hash ---
         * NMMiner: 15 NOPs, then l16ui check.
         * LOAD copies 8 words from internal state → SHA_TEXT ≈ 8-16 cycles. */
        NOP_15;

        /* Quick hash check (16-bit filter).
         * Read SHA_TEXT[7] lower 16 bits via l16ui (1 instruction, no memw).
         * Matches NMMiner Rev3: l16ui a8, a2, 28 + beqz.n */
        {
            uint32_t hash7_lo16;
            __asm__ volatile("l16ui %0, %1, 28" : "=r"(hash7_lo16) : "r"(base));
            if (hash7_lo16 == 0) {
                /* Candidate: full digest read + target comparison.
                 * Cold path isolated in noinline function to reduce register
                 * pressure in the hot loop (lets GCC keep b0 in registers). */
                if (HwCheckHashCandidate(Nonce, p_Job)) {
                    *p_Nonce = Nonce;
                    *p_Found = true;
                    esp_sha_unlock_engine(SHA2_256);
                    return PdqOk;
                }
            }
        }

        /* --- Phase 8: Fill block0[0:7] + START + nonce update ---
         * SHA_TEXT[8:15] already has block0[8:15] from Phase 5.
         * Uses cached registers for [0:5], memory for [6:7].
         *
         * START trigger in asm block with "memory" clobber ensures all data
         * writes (base[0:7]) are flushed to SHA peripheral BEFORE START fires.
         * Without this, GCC reorders trigger before base[7] (observed).
         *
         * NMMiner: 6 cached stores + 2 load/store pairs + START + 8 NOPs + jump.
         * Our code: same structure, nonce management replaces NOP_8 as delay. */
        base[0] = b0_0; base[1] = b0_1; base[2] = b0_2;
        base[3] = b0_3; base[4] = b0_4; base[5] = b0_5;
        base[6] = Block0[6]; base[7] = Block0[7];

        __asm__ volatile(
            "memw\n"
            "s32i %[trig], %[b], 144\n"  /* SHA256_START (block0 for next iter) */
            :: [trig] "r"(trigger), [b] "r"(base) : "memory");

        /* Nonce management replaces NOP_8: provides ~7 cycles of delay
         * between START and next iteration's SHA_TEXT writes.
         * NOP_8 ensures minimum 8 cycles before any SHA_TEXT access. */
        NOP_8;
        if (Nonce == NonceEnd) break;
        Nonce++;
    }

    esp_sha_unlock_engine(SHA2_256);
    return PdqOk;
}

/* ============================================================================
 * Mining loop correctness test: exercises the ACTUAL PdqSha256MineBlockHw
 * function (NOP-timed hot loop) using Bitcoin Block #1's known header/nonce.
 *
 * The single-hash boot test (PdqSha256HwCorrectnessTest) does NOT exercise
 * the loop structure (Phase 8→Phase 1 transition, nonce increment, branch
 * prediction, register pressure).  This test feeds a real block through the
 * real mining function and verifies it finds the correct nonce.
 *
 * If the NOPs are too aggressive, the HW will compute wrong hashes, the
 * correct nonce won't be found (or a false-positive will be SW-rejected).
 * ============================================================================ */
bool PdqSha256HwMiningLoopTest(void) {
    extern int printf(const char*, ...);

    /* Bitcoin block #1 header (80 bytes) — same test vector as boot test */
    static const uint8_t TestHeader[80] = {
        0x01,0x00,0x00,0x00,
        0x6f,0xe2,0x8c,0x0a,0xb6,0xf1,0xb3,0x72,0xc1,0xa6,0xa2,0x46,
        0xae,0x63,0xf7,0x4f,0x93,0x1e,0x83,0x65,0xe1,0x5a,0x08,0x9c,
        0x68,0xd6,0x19,0x00,0x00,0x00,0x00,0x00,
        0x98,0x20,0x51,0xfd,0x1e,0x4b,0xa7,0x44,0xbb,0xbe,0x68,0x0e,
        0x1f,0xee,0x14,0x67,0x7b,0xa1,0xa3,0xc3,0x54,0x0b,0xf7,0xb1,
        0xcd,0xb6,0x06,0xe8,0x57,0x23,0x3e,0x0e,
        0x61,0xbc,0x66,0x49,
        0xff,0xff,0x00,0x1d,
        0x01,0xe3,0x62,0x99
    };

    /* Block #1 actual nonce in native uint32 (LE): header bytes 01,E3,62,99
     * → LE uint32 0x9962E301.  Bswap32(0x9962E301) = 0x01E36299 for SHA. */
    const uint32_t KnownNonce = 0x9962E301;

    /* Build PdqMiningJob_t with HeaderSwapped (BE words + SHA padding) */
    PdqMiningJob_t Job;
    memset(&Job, 0, sizeof(Job));
    const uint32_t* HdrWords = (const uint32_t*)TestHeader;
    for (int i = 0; i < 20; i++)
        Job.HeaderSwapped[i] = __builtin_bswap32(HdrWords[i]);
    Job.HeaderSwapped[20] = 0x80000000;   /* SHA padding bit */
    for (int i = 21; i < 31; i++) Job.HeaderSwapped[i] = 0;
    Job.HeaderSwapped[31] = 0x00000280;   /* bit length = 640 */

    /* Difficulty 1 target: 0x00000000FFFF000000...00 */
    Job.Target[7] = 0x00000000;
    Job.Target[6] = 0xFFFF0000;
    /* Target[0..5] already 0 from memset */

    /* Search 50000 nonces before known nonce + 1000 after.
     * At ~876 KH/s this takes ~58 ms — fast enough for boot-time test. */
    Job.NonceStart = KnownNonce - 50000;
    Job.NonceEnd   = KnownNonce + 1000;
    strcpy(Job.JobId, "loop-test");

    uint32_t FoundNonce = 0;
    bool Found = false;

    printf("[LOOP-TEST] Mining %u nonces (0x%08X–0x%08X), expect nonce 0x%08X\n",
           (unsigned)(Job.NonceEnd - Job.NonceStart + 1),
           Job.NonceStart, Job.NonceEnd, KnownNonce);

    PdqError_t Err = PdqSha256MineBlockHw(&Job, &FoundNonce, &Found);
    if (Err != PdqOk) {
        printf("[LOOP-TEST] Result: FAIL (error %d)\n", Err);
        return false;
    }
    if (!Found) {
        printf("[LOOP-TEST] Result: FAIL (known nonce not found)\n");
        return false;
    }
    if (FoundNonce != KnownNonce) {
        printf("[LOOP-TEST] Result: FAIL (found 0x%08X, expected 0x%08X)\n",
               FoundNonce, KnownNonce);
        return false;
    }

    /* SW verify: reconstruct header with found nonce, compute SHA256d */
    uint8_t VerifyHdr[80];
    memcpy(VerifyHdr, TestHeader, 80);
    VerifyHdr[76] = (uint8_t)(FoundNonce);
    VerifyHdr[77] = (uint8_t)(FoundNonce >> 8);
    VerifyHdr[78] = (uint8_t)(FoundNonce >> 16);
    VerifyHdr[79] = (uint8_t)(FoundNonce >> 24);
    uint8_t SwHash[32];
    PdqSha256d(VerifyHdr, 80, SwHash);

    /* Confirm SW hash meets target */
    const uint32_t* SwW = (const uint32_t*)SwHash;
    for (int i = 7; i >= 0; i--) {
        if (SwW[i] > Job.Target[i]) {
            printf("[LOOP-TEST] Result: FAIL (SW hash doesn't meet target)\n");
            return false;
        }
        if (SwW[i] < Job.Target[i]) break;
    }

    printf("[LOOP-TEST] Result: PASS (nonce 0x%08X, SW verified)\n", FoundNonce);
    return true;
}

/* ============================================================================
 * HW SHA benchmark: measures actual per-nonce cycle counts and validates
 * the overlap/preservation assumptions used in the optimized mining loop.
 * ============================================================================ */
void PdqSha256HwDiagnostic(void) {
    extern int printf(const char*, ...);

    esp_sha_lock_engine(SHA2_256);

    volatile uint32_t* const StartReg    = (volatile uint32_t*)SHA_256_START_REG;
    volatile uint32_t* const ContinueReg = (volatile uint32_t*)SHA_256_CONTINUE_REG;
    volatile uint32_t* const LoadReg     = (volatile uint32_t*)SHA_256_LOAD_REG;
    volatile uint32_t* const BusyReg     = (volatile uint32_t*)SHA_256_BUSY_REG;
    uint32_t* TextNV = (uint32_t*)SHA_TEXT_BASE;
    volatile uint32_t* TextV  = (volatile uint32_t*)SHA_TEXT_BASE;

    /* Test data (deterministic) */
    uint32_t block0[16], block1[16];
    for (int i = 0; i < 16; i++) {
        block0[i]  = 0x01020304 + i * 0x11111111;
        block1[i]  = 0xA0B0C0D0 + i * 0x01010101;
    }

    /* === Test 1: SHA_TEXT preserved after START? === */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    bool preservedAfterStart = true;
    for (int i = 0; i < 16; i++) {
        if (TextV[i] != block0[i]) { preservedAfterStart = false; break; }
    }
    printf("[DIAG] SHA_TEXT preserved after START: %s\n",
           preservedAfterStart ? "YES" : "NO");

    /* === Test 2: SHA_TEXT preserved after CONTINUE? === */
    /* Re-fill block0 and restart, then fill block1 and continue */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    *ContinueReg = 1;
    while (*BusyReg) {}
    bool preservedAfterCont = true;
    for (int i = 0; i < 16; i++) {
        if (TextV[i] != block1[i]) { preservedAfterCont = false; break; }
    }
    printf("[DIAG] SHA_TEXT preserved after CONTINUE: %s\n",
           preservedAfterCont ? "YES" : "NO");

    /* === Test 3: LOAD only writes [0:7], preserves [8:15]? === */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    bool loadPreserves815 = true;
    for (int i = 8; i < 16; i++) {
        if (TextV[i] != block0[i]) { loadPreserves815 = false; break; }
    }
    printf("[DIAG] LOAD preserves SHA_TEXT[8:15]: %s\n",
           loadPreserves815 ? "YES" : "NO");

    /* === Test 4: Overlap validation — full mining cycle === */
    /* Normal approach (reference) */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    *ContinueReg = 1;
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t refHash[8];
    for (int i = 0; i < 8; i++) refHash[i] = TextV[i];

    /* Optimized approach (overlapped fills, reduced block0) */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i]; /* overlap block1 */
    while (*BusyReg) {}
    *ContinueReg = 1;
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    *StartReg = 1;
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i]; /* overlap block0 */
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t optHash[8];
    for (int i = 0; i < 8; i++) optHash[i] = TextV[i];

    bool overlapMatch = true;
    for (int i = 0; i < 8; i++) {
        if (refHash[i] != optHash[i]) { overlapMatch = false; break; }
    }
    printf("[DIAG] Overlap optimization correctness: %s\n",
           overlapMatch ? "PASS" : "FAIL");

    /* Second iteration using half-fill for block0 (relies on [8:15] preservation) */
    /* SHA_TEXT[8:15] should still have block0[8:15] from overlap fill above */
    for (int i = 0; i < 8; i++) TextNV[i] = block0[i]; /* only [0:7] */
    *StartReg = 1;
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i]; /* overlap block1 */
    while (*BusyReg) {}
    *ContinueReg = 1;
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    *StartReg = 1;
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i]; /* overlap block0 */
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t halfHash[8];
    for (int i = 0; i < 8; i++) halfHash[i] = TextV[i];

    bool halfMatch = true;
    for (int i = 0; i < 8; i++) {
        if (refHash[i] != halfHash[i]) { halfMatch = false; break; }
    }
    printf("[DIAG] Half-fill block0 correctness: %s\n",
           halfMatch ? "PASS" : "FAIL");

    /* === Test 5: Reduced padding (2 writes vs full 8 writes) ===
     * In the mining hot loop, block1 has zeros at positions [5:14] and
     * SHA_TEXT preserves these after CONTINUE+LOAD.  The hot loop only
     * writes TextNV[8]=0x80000000 and TextNV[15]=0x100 (2 writes).
     * Test with mining-realistic block1 data to validate. */
    {
        /* Mining-realistic block1: words 0-3 = header tail, 4=0x80, 5-14=0, 15=len */
        uint32_t mBlock1[16];
        mBlock1[0] = block1[0]; mBlock1[1] = block1[1];
        mBlock1[2] = block1[2]; mBlock1[3] = block1[3];
        mBlock1[4] = 0x80000000;
        for (int i = 5; i < 15; i++) mBlock1[i] = 0;
        mBlock1[15] = 0x00000280;

        /* Full 8-word padding (reference) */
        for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
        *StartReg = 1; while (*BusyReg) {}
        for (int i = 0; i < 16; i++) TextNV[i] = mBlock1[i];
        *ContinueReg = 1; while (*BusyReg) {}
        TextNV[8]  = 0x80000000;
        TextNV[9]  = 0; TextNV[10] = 0; TextNV[11] = 0;
        TextNV[12] = 0; TextNV[13] = 0; TextNV[14] = 0;
        TextNV[15] = 0x00000100;
        *LoadReg = 1; while (*BusyReg) {}
        *StartReg = 1; while (*BusyReg) {}
        *LoadReg = 1; while (*BusyReg) {}
        uint32_t fullPadHash[8];
        for (int i = 0; i < 8; i++) fullPadHash[i] = TextV[i];

        /* Reduced 2-write padding (hot loop approach) */
        for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
        *StartReg = 1; while (*BusyReg) {}
        for (int i = 0; i < 16; i++) TextNV[i] = mBlock1[i];
        *ContinueReg = 1; while (*BusyReg) {}
        /* Only 2 writes - [9:14] should be 0 from mBlock1 fill, preserved by LOAD */
        TextNV[8]  = 0x80000000;
        TextNV[15] = 0x00000100;
        *LoadReg = 1; while (*BusyReg) {}
        *StartReg = 1; while (*BusyReg) {}
        *LoadReg = 1; while (*BusyReg) {}
        uint32_t redPadHash[8];
        for (int i = 0; i < 8; i++) redPadHash[i] = TextV[i];

        bool padMatch = true;
        for (int i = 0; i < 8; i++) {
            if (fullPadHash[i] != redPadHash[i]) { padMatch = false; break; }
        }
        printf("[DIAG] Reduced padding (2 writes) correctness: %s\n",
               padMatch ? "PASS" : "FAIL");
    }

    /* === Test 6: Probe undocumented SHA_H registers at SHA_BASE+0x40 ===
     * On ESP32-S2/S3/C3, SHA_H_BASE = SHA_BASE + 0x40 allows midstate injection.
     * ESP32-D0 docs say this isn't supported, but the address gap 0x40-0x7F
     * exists and may contain the same H-state registers (undocumented).
     * If midstate injection works, we can skip block0 START for every nonce
     * (3 SHA blocks → 2 per nonce = ~50% HW speedup → ~1000 KH/s). */
    {
        /* Potential H-state register base (matches SHA_H_BASE on ESP32-S2) */
        volatile uint32_t* HReg = (volatile uint32_t*)(DR_REG_SHA_BASE + 0x40);

        /* Step 1: Compute reference: START(block0) + CONTINUE(block1) → LOAD → hash */
        for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
        *StartReg = 1;
        while (*BusyReg) {}
        for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
        *ContinueReg = 1;
        while (*BusyReg) {}
        *LoadReg = 1;
        while (*BusyReg) {}
        uint32_t hRef[8];
        for (int i = 0; i < 8; i++) hRef[i] = TextV[i];

        /* Step 2: Compute midstate: START(block0) → LOAD → save H state */
        for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
        *StartReg = 1;
        while (*BusyReg) {}
        *LoadReg = 1;
        while (*BusyReg) {}
        uint32_t midstate[8];
        for (int i = 0; i < 8; i++) midstate[i] = TextV[i];

        /* Step 3a: Read from 0x40-0x5F to see if they contain the H state */
        printf("[DIAG] Probing SHA_BASE+0x40 (undocumented H regs):\n");
        printf("[DIAG]   Read H[0:7] after LOAD:");
        for (int i = 0; i < 8; i++) printf(" %08lx", (unsigned long)HReg[i]);
        printf("\n");
        printf("[DIAG]   SHA_TEXT[0:7] (midstate):");
        for (int i = 0; i < 8; i++) printf(" %08lx", (unsigned long)midstate[i]);
        printf("\n");

        /* Check if H regs match midstate */
        bool hRegsMatchMidstate = true;
        for (int i = 0; i < 8; i++) {
            if (HReg[i] != midstate[i]) { hRegsMatchMidstate = false; break; }
        }
        printf("[DIAG]   H regs match midstate: %s\n",
               hRegsMatchMidstate ? "YES (H regs are readable!)" : "NO");

        /* Step 3b: Inject midstate and test CONTINUE */
        /* Corrupt internal state first: run a different START */
        uint32_t dummy[16];
        for (int i = 0; i < 16; i++) dummy[i] = 0xDEADBEEF;
        for (int i = 0; i < 16; i++) TextNV[i] = dummy[i];
        *StartReg = 1;
        while (*BusyReg) {}
        /* Internal state now has SHA256(0xDEADBEEF...) - different from our midstate */

        /* Attempt midstate injection: write saved midstate to H regs */
        for (int i = 0; i < 8; i++) HReg[i] = midstate[i];

        /* Now CONTINUE with block1 - if injection worked, result matches reference */
        for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
        *ContinueReg = 1;
        while (*BusyReg) {}
        *LoadReg = 1;
        while (*BusyReg) {}
        uint32_t hTest[8];
        for (int i = 0; i < 8; i++) hTest[i] = TextV[i];

        bool midstateInjectionWorks = true;
        for (int i = 0; i < 8; i++) {
            if (hRef[i] != hTest[i]) { midstateInjectionWorks = false; break; }
        }
        printf("[DIAG] *** MIDSTATE INJECTION (SHA_BASE+0x40): %s ***\n",
               midstateInjectionWorks ? "PASS (BREAKTHROUGH!)" : "FAIL");

        if (midstateInjectionWorks) {
            printf("[DIAG]   -> ESP32-D0 HAS undocumented SHA_H registers!\n");
            printf("[DIAG]   -> Can skip block0 START per nonce (3→2 blocks)\n");
            printf("[DIAG]   -> Expected HW hashrate: ~1000+ KH/s!\n");
        }

        /* Step 4: Also probe SHA_BASE+0x60-0x7F (alternate offset) */
        volatile uint32_t* HAlt = (volatile uint32_t*)(DR_REG_SHA_BASE + 0x60);
        printf("[DIAG]   SHA_BASE+0x60 probe:");
        for (int i = 0; i < 8; i++) printf(" %08lx", (unsigned long)HAlt[i]);
        printf("\n");
    }

    /* === Test 7: LOAD bidirectional test (midstate injection via SHA_TEXT) ===
     * The ESP32 TRM says: "Write 1 to load message_digest FROM SHA text memory
     * TO SHA engine" — this suggests LOAD writes SHA_TEXT[0:7] INTO the engine's
     * internal H state, not just reading it out.
     *
     * If LOAD is bidirectional (SHA_TEXT → engine H, then engine H → SHA_TEXT),
     * we can inject midstate by:
     *   1. Write midstate to SHA_TEXT[0:7]
     *   2. Trigger LOAD (sets engine H = midstate)
     *   3. Write block1 to SHA_TEXT
     *   4. CONTINUE (compress(midstate, block1))
     * This reduces mining from 3→2 HW blocks per nonce = ~1000+ KH/s! */
    {
        printf("[DIAG] Testing LOAD bidirectional (midstate via SHA_TEXT):\n");

        /* Step 1: Compute reference = compress(compress(IV, block0), block1) */
        for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
        *StartReg = 1;
        while (*BusyReg) {}
        for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
        *ContinueReg = 1;
        while (*BusyReg) {}
        *LoadReg = 1;
        while (*BusyReg) {}
        uint32_t bidirRef[8];
        for (int i = 0; i < 8; i++) bidirRef[i] = TextV[i];

        /* Step 2: Compute midstate = compress(IV, block0) via START+LOAD */
        for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
        *StartReg = 1;
        while (*BusyReg) {}
        *LoadReg = 1;
        while (*BusyReg) {}
        uint32_t bidirMid[8];
        for (int i = 0; i < 8; i++) bidirMid[i] = TextV[i];

        /* Step 3: Corrupt engine state with completely different START */
        for (int i = 0; i < 16; i++) TextNV[i] = 0xDEADBEEF;
        *StartReg = 1;
        while (*BusyReg) {}
        /* Engine H now = compress(IV, 0xDEADBEEF x16) — NOT our midstate */

        /* Step 4: Write midstate to SHA_TEXT[0:7] and trigger LOAD
         * If LOAD is bidirectional: engine H = midstate (from SHA_TEXT) */
        for (int i = 0; i < 8; i++) TextNV[i] = bidirMid[i];
        *LoadReg = 1;
        while (*BusyReg) {}

        /* Step 5: Write block1 and CONTINUE using (hopefully) injected midstate */
        for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
        *ContinueReg = 1;
        while (*BusyReg) {}
        *LoadReg = 1;
        while (*BusyReg) {}
        uint32_t bidirTest[8];
        for (int i = 0; i < 8; i++) bidirTest[i] = TextV[i];

        /* Step 6: Compare — if LOAD injected the midstate, results match */
        bool loadBidir = true;
        for (int i = 0; i < 8; i++) {
            if (bidirRef[i] != bidirTest[i]) { loadBidir = false; break; }
        }
        printf("[DIAG] *** LOAD BIDIRECTIONAL (midstate via SHA_TEXT): %s ***\n",
               loadBidir ? "PASS (BREAKTHROUGH!)" : "FAIL");

        if (loadBidir) {
            printf("[DIAG]   -> LOAD sets engine H from SHA_TEXT[0:7]!\n");
            printf("[DIAG]   -> Midstate injection: write SHA_TEXT + LOAD + CONTINUE\n");
            printf("[DIAG]   -> Mining reduced from 3 to 2 HW blocks per nonce!\n");

            /* Benchmark: 2-block mining (midstate pre-loaded) */
            const int N2 = 10000;
            uint32_t t2a, t2b;
            /* Warm up */
            for (int n = 0; n < 100; n++) {
                for (int i = 0; i < 8; i++) TextNV[i] = bidirMid[i];
                *LoadReg = 1;
                while (*BusyReg) {}
                for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
                TextNV[3] = __builtin_bswap32(n);
                *ContinueReg = 1;
                while (*BusyReg) {}
                *LoadReg = 1;
                TextNV[8]  = 0x80000000;
                TextNV[15] = 0x00000100;
                while (*BusyReg) {}
                *StartReg = 1;
                while (*BusyReg) {}
                *LoadReg = 1;
                while (*BusyReg) {}
            }
            __asm__ volatile("rsr %0, ccount" : "=a"(t2a));
            for (int n = 0; n < N2; n++) {
                /* Inject midstate: write to SHA_TEXT + LOAD */
                for (int i = 0; i < 8; i++) TextNV[i] = bidirMid[i];
                *LoadReg = 1;
                while (*BusyReg) {}
                /* Block1 with nonce */
                for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
                TextNV[3] = __builtin_bswap32(n);
                *ContinueReg = 1;
                while (*BusyReg) {}
                /* Double hash */
                *LoadReg = 1;
                TextNV[8]  = 0x80000000;
                TextNV[15] = 0x00000100;
                while (*BusyReg) {}
                *StartReg = 1;
                while (*BusyReg) {}
                *LoadReg = 1;
                while (*BusyReg) {}
            }
            __asm__ volatile("rsr %0, ccount" : "=a"(t2b));
            uint32_t Total2 = t2b - t2a;
            uint32_t PerNonce2 = Total2 / N2;
            uint32_t Khs2 = 240000000UL / PerNonce2 / 1000;
            printf("[DIAG] 2-BLOCK %d nonces: %lu total, %lu cyc/nonce, ~%lu KH/s (HW)\n",
                   N2, (unsigned long)Total2, (unsigned long)PerNonce2, (unsigned long)Khs2);
        } else {
            printf("[DIAG]   Expected: ");
            for (int i = 0; i < 4; i++) printf("%08lx ", (unsigned long)bidirRef[i]);
            printf("\n[DIAG]   Got:      ");
            for (int i = 0; i < 4; i++) printf("%08lx ", (unsigned long)bidirTest[i]);
            printf("\n");
        }
    }

    /* === Timing: individual operations === */
    uint32_t t0, t1;
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    __asm__ volatile("rsr %0, ccount" : "=a"(t0));
    *StartReg = 1;
    while (*BusyReg) {}
    __asm__ volatile("rsr %0, ccount" : "=a"(t1));
    printf("[DIAG] START: %lu cyc  CONTINUE: ", (unsigned long)(t1 - t0));

    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    __asm__ volatile("rsr %0, ccount" : "=a"(t0));
    *ContinueReg = 1;
    while (*BusyReg) {}
    __asm__ volatile("rsr %0, ccount" : "=a"(t1));
    printf("%lu cyc  LOAD: ", (unsigned long)(t1 - t0));

    __asm__ volatile("rsr %0, ccount" : "=a"(t0));
    *LoadReg = 1;
    while (*BusyReg) {}
    __asm__ volatile("rsr %0, ccount" : "=a"(t1));
    printf("%lu cyc\n", (unsigned long)(t1 - t0));

    /* === Test 6: Operation queuing ===
     * Can we write CONTINUE while START is still busy?
     * If so, the engine would auto-chain: START → CONTINUE with zero gap. */

    /* Reference: normal START → wait → CONTINUE */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    *ContinueReg = 1;
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t queueRef[8];
    for (int i = 0; i < 8; i++) queueRef[i] = TextV[i];

    /* Test A: Queue CONTINUE during START (write block1 then ContinueReg before START finishes) */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i]; /* overlap */
    *ContinueReg = 1;  /* queue CONTINUE while START may still be busy */
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t queueA[8];
    for (int i = 0; i < 8; i++) queueA[i] = TextV[i];
    bool queueContMatch = true;
    for (int i = 0; i < 8; i++) {
        if (queueRef[i] != queueA[i]) { queueContMatch = false; break; }
    }
    printf("[DIAG] Queue CONTINUE during START: %s\n",
           queueContMatch ? "PASS (chaining works!)" : "FAIL (not supported)");

    /* Test B: Queue LOAD during CONTINUE */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    *ContinueReg = 1;
    *LoadReg = 1;  /* queue LOAD while CONTINUE may still be busy */
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    /* LOAD should have already copied digest to [0:7] */
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t queueB[8];
    for (int i = 0; i < 8; i++) queueB[i] = TextV[i];
    bool queueLoadMatch = true;
    for (int i = 0; i < 8; i++) {
        if (queueRef[i] != queueB[i]) { queueLoadMatch = false; break; }
    }
    printf("[DIAG] Queue LOAD during CONTINUE: %s\n",
           queueLoadMatch ? "PASS (chaining works!)" : "FAIL (not supported)");

    /* Test C: Queue START during LOAD */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    *ContinueReg = 1;
    while (*BusyReg) {}
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    /* Now LOAD is done. Try queue START during a new LOAD: */
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    *StartReg = 1;  /* queue START during final LOAD */
    while (*BusyReg) {}
    /* If queued START ran, engine would have processed SHA_TEXT again (wrong) */
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t queueC[8];
    for (int i = 0; i < 8; i++) queueC[i] = TextV[i];
    bool queueStartMatch = true;
    for (int i = 0; i < 8; i++) {
        if (queueRef[i] != queueC[i]) { queueStartMatch = false; break; }
    }
    printf("[DIAG] Queue START during LOAD: %s (expect FAIL if queued)\n",
           queueStartMatch ? "PASS (ignored)" : "FAIL (queued - unexpected)");

    /* === Timing: full-chain test (if queuing works) === */
    {
        const int N = 10000;
        /* Warm up */
        for (int n = 0; n < 100; n++) {
            for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
            *StartReg = 1;
            for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
            while (*BusyReg) {}
            *ContinueReg = 1;
            while (*BusyReg) {}
            *LoadReg = 1;
            TextNV[8]  = 0x80000000;
            TextNV[15] = 0x00000100;
            while (*BusyReg) {}
            *StartReg = 1;
            TextNV[8]  = block0[8]; TextNV[9]  = block0[9];
            TextNV[10] = block0[10]; TextNV[11] = block0[11];
            TextNV[12] = block0[12]; TextNV[13] = block0[13];
            TextNV[14] = block0[14]; TextNV[15] = block0[15];
            while (*BusyReg) {}
            *LoadReg = 1;
            while (*BusyReg) {}
        }

        /* Measure N iterations of the optimized hot loop */
        __asm__ volatile("rsr %0, ccount" : "=a"(t0));
        for (int n = 0; n < N; n++) {
            for (int i = 0; i < 8; i++) TextNV[i] = block0[i];
            *StartReg = 1;
            for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
            TextNV[3] = __builtin_bswap32(n);
            while (*BusyReg) {}
            *ContinueReg = 1;
            while (*BusyReg) {}
            *LoadReg = 1;
            TextNV[8]  = 0x80000000;
            TextNV[15] = 0x00000100;
            while (*BusyReg) {}
            *StartReg = 1;
            TextNV[8]  = block0[8]; TextNV[9]  = block0[9];
            TextNV[10] = block0[10]; TextNV[11] = block0[11];
            TextNV[12] = block0[12]; TextNV[13] = block0[13];
            TextNV[14] = block0[14]; TextNV[15] = block0[15];
            while (*BusyReg) {}
            *LoadReg = 1;
            while (*BusyReg) {}
        }
        __asm__ volatile("rsr %0, ccount" : "=a"(t1));

        uint32_t Total = t1 - t0;
        uint32_t PerNonce = Total / N;
        uint32_t Khs = 240000000UL / PerNonce / 1000;
        printf("[DIAG] Batch %d nonces: %lu total, %lu cyc/nonce, ~%lu KH/s (HW only)\n",
               N, (unsigned long)Total, (unsigned long)PerNonce, (unsigned long)Khs);

        /* If queuing works, measure chained version (START→CONTINUE + LOAD→START) */
        if (queueContMatch) {
            __asm__ volatile("rsr %0, ccount" : "=a"(t0));
            for (int n = 0; n < N; n++) {
                for (int i = 0; i < 8; i++) TextNV[i] = block0[i];
                *StartReg = 1;
                for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
                TextNV[3] = __builtin_bswap32(n);
                *ContinueReg = 1;  /* chained during START */
                while (*BusyReg) {}
                *LoadReg = 1;
                TextNV[8]  = 0x80000000;
                TextNV[15] = 0x00000100;
                *StartReg = 1;  /* chained during LOAD */
                while (*BusyReg) {}
                *LoadReg = 1;
                TextNV[8]  = block0[8]; TextNV[9]  = block0[9];
                TextNV[10] = block0[10]; TextNV[11] = block0[11];
                TextNV[12] = block0[12]; TextNV[13] = block0[13];
                TextNV[14] = block0[14]; TextNV[15] = block0[15];
                while (*BusyReg) {}
            }
            __asm__ volatile("rsr %0, ccount" : "=a"(t1));

            Total = t1 - t0;
            PerNonce = Total / N;
            Khs = 240000000UL / PerNonce / 1000;
            printf("[DIAG] CHAINED %d nonces: %lu total, %lu cyc/nonce, ~%lu KH/s (HW only)\n",
                   N, (unsigned long)Total, (unsigned long)PerNonce, (unsigned long)Khs);
        }
    }

    esp_sha_unlock_engine(SHA2_256);
}

#else
/* Non-ESP32 stub: fall back to software mining */
PdqError_t PdqSha256MineBlockHw(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    return PdqSha256MineBlock(p_Job, p_Nonce, p_Found);
}
void PdqSha256HwDiagnostic(void) {}
bool PdqSha256HwCorrectnessTest(void) { return true; }
bool PdqSha256HwMiningLoopTest(void) { return true; }
#endif
