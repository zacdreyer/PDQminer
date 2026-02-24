/**
 * @file sha256_engine.c
 * @brief High-performance SHA256 engine optimized for Bitcoin mining
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "sha256_engine.h"
#include <string.h>

/* Use -Os for the SW SHA256 engine: on Xtensa LX6 (16 registers with windows),
 * -O3 causes massive register spilling in the unrolled SHA256 rounds.
 * Also disable -funroll-loops which survives the pragma from the command line.
 * We use push/pop to restore -O2 for the HW mining code later in this file. */
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
    for (int i = 0; i < 8; i++) {
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
    uint32_t W[64] = { p_Bake[0], p_Bake[1], p_Bake[2], ReadBe32(p_BlockTail + 12),
                       0x80000000, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 640 };
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

    uint32_t TargetHigh = p_Job->Target[0];

    uint32_t Nonce = p_Job->NonceStart;
    for (;;) {
        /* Write nonce into block tail (big-endian) */
        WriteBe32(BlockTail + 12, Nonce);

        /* Run double SHA256 with baked pre-computation */
        uint32_t FinalState[8];
        if (PdqSha256dBaked(MidState, BlockTail, Bake, FinalState)) {
            /* Passed early termination - do full target check */
            if (FinalState[0] <= TargetHigh) {
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
 *   5. Compiled at -O2 (restored from -Os used for SW SHA code)
 * ============================================================================ */

/* Restore -O2 for HW mining code (SW SHA used -Os to avoid register spilling) */
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
PDQ_IRAM_ATTR static inline __attribute__((always_inline)) void HwShaFillBlock0Half(const uint32_t* p_Data) {
    uint32_t* Reg = (uint32_t*)SHA_TEXT_BASE;
    Reg[0]  = p_Data[0];  Reg[1]  = p_Data[1];
    Reg[2]  = p_Data[2];  Reg[3]  = p_Data[3];
    Reg[4]  = p_Data[4];  Reg[5]  = p_Data[5];
    Reg[6]  = p_Data[6];  Reg[7]  = p_Data[7];
}

PdqError_t PdqSha256MineBlockHw(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    if (p_Job == NULL || p_Nonce == NULL || p_Found == NULL) return PdqErrorInvalidParam;
    *p_Found = false;

    /* HeaderSwapped contains the 80-byte header as 20 big-endian words + padding to 32 words.
     * Words 0-15 = first SHA block, words 16-31 = second SHA block. */
    const uint32_t* Block0 = p_Job->HeaderSwapped;      /* first 64 bytes (words 0-15) */
    const uint32_t* Block1 = p_Job->HeaderSwapped + 16;  /* bytes 64-127 (words 16-19 + padding) */

    uint32_t TargetHigh = p_Job->Target[0];

    /* Direct register pointers - avoid sha_ll_* computed addresses in hot loop.
     * SHA_256 = SHA_1 + 0x20 offset. */
    volatile uint32_t* const StartReg    = (volatile uint32_t*)SHA_256_START_REG;
    volatile uint32_t* const ContinueReg = (volatile uint32_t*)SHA_256_CONTINUE_REG;
    volatile uint32_t* const LoadReg     = (volatile uint32_t*)SHA_256_LOAD_REG;
    uint32_t* const TextNV = (uint32_t*)SHA_TEXT_BASE;

    esp_sha_lock_engine(SHA2_256);

    uint32_t Nonce = p_Job->NonceStart;
    uint32_t NonceEnd = p_Job->NonceEnd;
    uint32_t FinalHash[8];

    /* === First iteration: full block0 fill (16 words) since SHA_TEXT is cold === */
    HwShaFillBlock(Block0);
    *StartReg = 1;
    /* OVERLAP: fill block1 while block0 START runs (safe: START latches data at trigger) */
    HwShaFillUpperBlock(Block1, __builtin_bswap32(Nonce));
    HwShaWaitIdle();

    *ContinueReg = 1;
    HwShaWaitIdle();

    /* Double-hash padding: after CONTINUE, SHA_TEXT still has block1 data.
     * block1[8:15] = {0, 0, 0, 0, 0, 0, 0, 0x280}
     * We need:       {0x80000000, 0, 0, 0, 0, 0, 0, 0x100}
     * Only words [8] and [15] differ. */
    TextNV[8]  = 0x80000000;
    TextNV[15] = 0x00000100;

    /* LOAD intermediate hash to SHA_TEXT[0:7] */
    *LoadReg = 1;
    HwShaWaitIdle();

    /* Second SHA256: SHA_TEXT = [intermediate_hash, double_padding] */
    *StartReg = 1;
    /* OVERLAP: fill block0 for next iteration while 2nd hash runs.
     * After this, SHA_TEXT[0:15] = block0. Then LOAD overwrites only [0:7]
     * with the final hash, leaving SHA_TEXT[8:15] = block0[8:15] intact. */
    HwShaFillBlock(Block0);
    HwShaWaitIdle();

    *LoadReg = 1;
    /* Read final hash (APB stalls until LOAD completes) */
    if (HwShaReadDigestSwapIf(FinalHash)) {
        if (FinalHash[0] <= TargetHigh) {
            if (CheckTarget(FinalHash, p_Job->Target)) {
                *p_Nonce = Nonce;
                *p_Found = true;
                esp_sha_unlock_engine(SHA2_256);
                return PdqOk;
            }
        }
    }

    if (Nonce == NonceEnd) { esp_sha_unlock_engine(SHA2_256); return PdqOk; }
    Nonce++;

    /* === Hot loop: SHA_TEXT[8:15] already has block0[8:15] from overlap === */
    for (;;) {
        /* Block0: only fill [0:7] since [8:15] is preserved from overlap fill */
        HwShaFillBlock0Half(Block0);
        *StartReg = 1;
        /* OVERLAP: fill block1 during block0 START */
        HwShaFillUpperBlock(Block1, __builtin_bswap32(Nonce));
        HwShaWaitIdle();

        *ContinueReg = 1;
        HwShaWaitIdle();

        /* Minimal double-hash padding: only 2 words differ from block1 residual */
        TextNV[8]  = 0x80000000;
        TextNV[15] = 0x00000100;

        *LoadReg = 1;
        HwShaWaitIdle();

        *StartReg = 1;
        /* OVERLAP: fill block0 for next iteration during 2nd hash */
        HwShaFillBlock(Block0);
        HwShaWaitIdle();

        *LoadReg = 1;

        if (HwShaReadDigestSwapIf(FinalHash)) {
            if (FinalHash[0] <= TargetHigh) {
                if (CheckTarget(FinalHash, p_Job->Target)) {
                    *p_Nonce = Nonce;
                    *p_Found = true;
                    esp_sha_unlock_engine(SHA2_256);
                    return PdqOk;
                }
            }
        }

        if (Nonce == NonceEnd) break;
        Nonce++;
    }

    esp_sha_unlock_engine(SHA2_256);
    return PdqOk;
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

    /* === Test 5: Reduced padding (2 writes) correctness === */
    /* Reference: full 8-word padding after CONTINUE */
    for (int i = 0; i < 16; i++) TextNV[i] = block0[i];
    *StartReg = 1;
    while (*BusyReg) {}
    for (int i = 0; i < 16; i++) TextNV[i] = block1[i];
    *ContinueReg = 1;
    while (*BusyReg) {}
    /* Full padding fill */
    TextNV[8]  = 0x80000000;
    TextNV[9]  = 0; TextNV[10] = 0; TextNV[11] = 0;
    TextNV[12] = 0; TextNV[13] = 0; TextNV[14] = 0;
    TextNV[15] = 0x00000100;
    *LoadReg = 1;
    while (*BusyReg) {}
    *StartReg = 1;
    while (*BusyReg) {}
    *LoadReg = 1;
    while (*BusyReg) {}
    uint32_t fullPadHash[8];
    for (int i = 0; i < 8; i++) fullPadHash[i] = TextV[i];

    bool padMatch = true;
    for (int i = 0; i < 8; i++) {
        if (refHash[i] != fullPadHash[i]) { padMatch = false; break; }
    }
    printf("[DIAG] Reduced padding (2 writes) correctness: %s\n",
           padMatch ? "PASS" : "FAIL");

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

    esp_sha_unlock_engine(SHA2_256);
}

#else
/* Non-ESP32 stub: fall back to software mining */
PdqError_t PdqSha256MineBlockHw(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    return PdqSha256MineBlock(p_Job, p_Nonce, p_Found);
}
void PdqSha256HwDiagnostic(void) {}
#endif
