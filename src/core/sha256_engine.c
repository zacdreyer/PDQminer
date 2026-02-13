/**
 * @file sha256_engine.c
 * @brief High-performance SHA256 engine optimized for Bitcoin mining
 * @copyright Copyright (c) 2025 PDQminer Contributors
 * @license GPL-3.0
 */

#include "sha256_engine.h"
#include <string.h>

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

static PDQ_DRAM_ATTR const uint32_t K2_8  = 0xd807aa98 + 0x80000000;
static PDQ_DRAM_ATTR const uint32_t K2_9  = 0x12835b01;
static PDQ_DRAM_ATTR const uint32_t K2_10 = 0x243185be;
static PDQ_DRAM_ATTR const uint32_t K2_11 = 0x550c7dc3;
static PDQ_DRAM_ATTR const uint32_t K2_12 = 0x72be5d74;
static PDQ_DRAM_ATTR const uint32_t K2_13 = 0x80deb1fe;
static PDQ_DRAM_ATTR const uint32_t K2_14 = 0x9bdc06a7;
static PDQ_DRAM_ATTR const uint32_t K2_15 = 0xc19bf174 + 256;

static const uint32_t H_INIT[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
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

PDQ_IRAM_ATTR static void Sha256Transform(uint32_t* p_State, const uint8_t* p_Block) {
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

PDQ_IRAM_ATTR static void Sha256TransformW(uint32_t* p_State, const uint32_t* p_W) {
    register uint32_t a, b, c, d, e, f, g, h;

    a = p_State[0]; b = p_State[1]; c = p_State[2]; d = p_State[3];
    e = p_State[4]; f = p_State[5]; g = p_State[6]; h = p_State[7];

    SHA256_ROUND(a,b,c,d,e,f,g,h, K[0], p_W[0]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[1], p_W[1]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[2], p_W[2]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[3], p_W[3]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[4], p_W[4]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[5], p_W[5]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[6], p_W[6]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[7], p_W[7]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[8], p_W[8]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[9], p_W[9]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[10], p_W[10]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[11], p_W[11]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[12], p_W[12]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[13], p_W[13]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[14], p_W[14]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[15], p_W[15]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[16], p_W[16]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[17], p_W[17]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[18], p_W[18]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[19], p_W[19]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[20], p_W[20]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[21], p_W[21]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[22], p_W[22]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[23], p_W[23]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[24], p_W[24]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[25], p_W[25]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[26], p_W[26]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[27], p_W[27]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[28], p_W[28]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[29], p_W[29]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[30], p_W[30]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[31], p_W[31]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[32], p_W[32]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[33], p_W[33]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[34], p_W[34]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[35], p_W[35]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[36], p_W[36]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[37], p_W[37]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[38], p_W[38]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[39], p_W[39]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[40], p_W[40]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[41], p_W[41]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[42], p_W[42]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[43], p_W[43]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[44], p_W[44]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[45], p_W[45]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[46], p_W[46]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[47], p_W[47]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[48], p_W[48]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[49], p_W[49]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[50], p_W[50]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[51], p_W[51]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[52], p_W[52]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[53], p_W[53]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[54], p_W[54]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[55], p_W[55]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[56], p_W[56]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[57], p_W[57]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[58], p_W[58]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[59], p_W[59]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[60], p_W[60]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[61], p_W[61]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[62], p_W[62]);
    SHA256_ROUND(a,b,c,d,e,f,g,h, K[63], p_W[63]);

    p_State[0] += a; p_State[1] += b; p_State[2] += c; p_State[3] += d;
    p_State[4] += e; p_State[5] += f; p_State[6] += g; p_State[7] += h;
}

__attribute__((hot)) PDQ_IRAM_ATTR PdqError_t PdqSha256MineBlock(const PdqMiningJob_t* p_Job, uint32_t* p_Nonce, bool* p_Found) {
    if (p_Job == NULL || p_Nonce == NULL || p_Found == NULL) return PdqErrorInvalidParam;

    *p_Found = false;

    uint32_t MidState[8];
    for (int i = 0; i < 8; i++) {
        MidState[i] = ReadBe32(p_Job->Midstate + i * 4);
    }

    uint32_t W1_0 = ReadBe32(p_Job->BlockTail);
    uint32_t W1_1 = ReadBe32(p_Job->BlockTail + 4);
    uint32_t W1_2 = ReadBe32(p_Job->BlockTail + 8);
    uint32_t W1_4 = ReadBe32(p_Job->BlockTail + 16);
    uint32_t W1_5 = ReadBe32(p_Job->BlockTail + 20);
    uint32_t W1_6 = ReadBe32(p_Job->BlockTail + 24);
    uint32_t W1_7 = ReadBe32(p_Job->BlockTail + 28);
    uint32_t W1_8 = ReadBe32(p_Job->BlockTail + 32);
    uint32_t W1_9 = ReadBe32(p_Job->BlockTail + 36);
    uint32_t W1_10 = ReadBe32(p_Job->BlockTail + 40);
    uint32_t W1_11 = ReadBe32(p_Job->BlockTail + 44);
    uint32_t W1_12 = ReadBe32(p_Job->BlockTail + 48);
    uint32_t W1_13 = ReadBe32(p_Job->BlockTail + 52);
    uint32_t W1_14 = ReadBe32(p_Job->BlockTail + 56);
    uint32_t W1_15 = ReadBe32(p_Job->BlockTail + 60);

    uint32_t W1Pre16 = SIG1(W1_14) + W1_9 + SIG0(W1_1) + W1_0;
    uint32_t W1Pre17 = SIG1(W1_15) + W1_10 + SIG0(W1_2) + W1_1;
    uint32_t W1Pre18Base = SIG1(W1Pre16) + W1_11 + W1_2;
    uint32_t W1Pre19Base = SIG1(W1Pre17) + W1_12;

    uint32_t TargetHigh = p_Job->Target[0];

    static const uint32_t W2_8 = 0x80000000;
    static const uint32_t W2_15 = 256;
    static const uint32_t W2_SIG1_15 = 0x00A00000;
    static const uint32_t W2_SIG0_8 = 0x11002000;

    for (uint32_t Nonce = p_Job->NonceStart; Nonce <= p_Job->NonceEnd; Nonce++) {
        uint32_t W1_3 = __builtin_bswap32(Nonce);
        uint32_t Sig0_3 = SIG0(W1_3);

        uint32_t W1_16 = W1Pre16;
        uint32_t W1_17 = W1Pre17;
        uint32_t W1_18 = W1Pre18Base + Sig0_3;
        uint32_t W1_19 = W1Pre19Base + SIG0(W1_4) + W1_3;
        uint32_t W1_20 = SIG1(W1_18) + W1_13 + SIG0(W1_5) + W1_4;
        uint32_t W1_21 = SIG1(W1_19) + W1_14 + SIG0(W1_6) + W1_5;
        uint32_t W1_22 = SIG1(W1_20) + W1_15 + SIG0(W1_7) + W1_6;
        uint32_t W1_23 = SIG1(W1_21) + W1_16 + SIG0(W1_8) + W1_7;
        uint32_t W1_24 = SIG1(W1_22) + W1_17 + SIG0(W1_9) + W1_8;
        uint32_t W1_25 = SIG1(W1_23) + W1_18 + SIG0(W1_10) + W1_9;
        uint32_t W1_26 = SIG1(W1_24) + W1_19 + SIG0(W1_11) + W1_10;
        uint32_t W1_27 = SIG1(W1_25) + W1_20 + SIG0(W1_12) + W1_11;
        uint32_t W1_28 = SIG1(W1_26) + W1_21 + SIG0(W1_13) + W1_12;
        uint32_t W1_29 = SIG1(W1_27) + W1_22 + SIG0(W1_14) + W1_13;
        uint32_t W1_30 = SIG1(W1_28) + W1_23 + SIG0(W1_15) + W1_14;
        uint32_t W1_31 = SIG1(W1_29) + W1_24 + SIG0(W1_16) + W1_15;
        uint32_t W1_32 = SIG1(W1_30) + W1_25 + SIG0(W1_17) + W1_16;
        uint32_t W1_33 = SIG1(W1_31) + W1_26 + SIG0(W1_18) + W1_17;
        uint32_t W1_34 = SIG1(W1_32) + W1_27 + SIG0(W1_19) + W1_18;
        uint32_t W1_35 = SIG1(W1_33) + W1_28 + SIG0(W1_20) + W1_19;
        uint32_t W1_36 = SIG1(W1_34) + W1_29 + SIG0(W1_21) + W1_20;
        uint32_t W1_37 = SIG1(W1_35) + W1_30 + SIG0(W1_22) + W1_21;
        uint32_t W1_38 = SIG1(W1_36) + W1_31 + SIG0(W1_23) + W1_22;
        uint32_t W1_39 = SIG1(W1_37) + W1_32 + SIG0(W1_24) + W1_23;
        uint32_t W1_40 = SIG1(W1_38) + W1_33 + SIG0(W1_25) + W1_24;
        uint32_t W1_41 = SIG1(W1_39) + W1_34 + SIG0(W1_26) + W1_25;
        uint32_t W1_42 = SIG1(W1_40) + W1_35 + SIG0(W1_27) + W1_26;
        uint32_t W1_43 = SIG1(W1_41) + W1_36 + SIG0(W1_28) + W1_27;
        uint32_t W1_44 = SIG1(W1_42) + W1_37 + SIG0(W1_29) + W1_28;
        uint32_t W1_45 = SIG1(W1_43) + W1_38 + SIG0(W1_30) + W1_29;
        uint32_t W1_46 = SIG1(W1_44) + W1_39 + SIG0(W1_31) + W1_30;
        uint32_t W1_47 = SIG1(W1_45) + W1_40 + SIG0(W1_32) + W1_31;
        uint32_t W1_48 = SIG1(W1_46) + W1_41 + SIG0(W1_33) + W1_32;
        uint32_t W1_49 = SIG1(W1_47) + W1_42 + SIG0(W1_34) + W1_33;
        uint32_t W1_50 = SIG1(W1_48) + W1_43 + SIG0(W1_35) + W1_34;
        uint32_t W1_51 = SIG1(W1_49) + W1_44 + SIG0(W1_36) + W1_35;
        uint32_t W1_52 = SIG1(W1_50) + W1_45 + SIG0(W1_37) + W1_36;
        uint32_t W1_53 = SIG1(W1_51) + W1_46 + SIG0(W1_38) + W1_37;
        uint32_t W1_54 = SIG1(W1_52) + W1_47 + SIG0(W1_39) + W1_38;
        uint32_t W1_55 = SIG1(W1_53) + W1_48 + SIG0(W1_40) + W1_39;
        uint32_t W1_56 = SIG1(W1_54) + W1_49 + SIG0(W1_41) + W1_40;
        uint32_t W1_57 = SIG1(W1_55) + W1_50 + SIG0(W1_42) + W1_41;
        uint32_t W1_58 = SIG1(W1_56) + W1_51 + SIG0(W1_43) + W1_42;
        uint32_t W1_59 = SIG1(W1_57) + W1_52 + SIG0(W1_44) + W1_43;
        uint32_t W1_60 = SIG1(W1_58) + W1_53 + SIG0(W1_45) + W1_44;
        uint32_t W1_61 = SIG1(W1_59) + W1_54 + SIG0(W1_46) + W1_45;
        uint32_t W1_62 = SIG1(W1_60) + W1_55 + SIG0(W1_47) + W1_46;
        uint32_t W1_63 = SIG1(W1_61) + W1_56 + SIG0(W1_48) + W1_47;

        register uint32_t a = MidState[0], b = MidState[1], c = MidState[2], d = MidState[3];
        register uint32_t e = MidState[4], f = MidState[5], g = MidState[6], h = MidState[7];

        SHA256_ROUND(a,b,c,d,e,f,g,h, K[0], W1_0);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[1], W1_1);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[2], W1_2);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[3], W1_3);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[4], W1_4);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[5], W1_5);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[6], W1_6);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[7], W1_7);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[8], W1_8);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[9], W1_9);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[10], W1_10);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[11], W1_11);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[12], W1_12);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[13], W1_13);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[14], W1_14);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[15], W1_15);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[16], W1_16);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[17], W1_17);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[18], W1_18);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[19], W1_19);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[20], W1_20);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[21], W1_21);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[22], W1_22);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[23], W1_23);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[24], W1_24);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[25], W1_25);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[26], W1_26);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[27], W1_27);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[28], W1_28);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[29], W1_29);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[30], W1_30);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[31], W1_31);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[32], W1_32);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[33], W1_33);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[34], W1_34);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[35], W1_35);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[36], W1_36);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[37], W1_37);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[38], W1_38);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[39], W1_39);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[40], W1_40);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[41], W1_41);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[42], W1_42);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[43], W1_43);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[44], W1_44);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[45], W1_45);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[46], W1_46);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[47], W1_47);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[48], W1_48);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[49], W1_49);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[50], W1_50);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[51], W1_51);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[52], W1_52);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[53], W1_53);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[54], W1_54);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[55], W1_55);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[56], W1_56);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[57], W1_57);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[58], W1_58);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[59], W1_59);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[60], W1_60);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[61], W1_61);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[62], W1_62);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[63], W1_63);

        uint32_t S0 = MidState[0] + a, S1 = MidState[1] + b;
        uint32_t S2 = MidState[2] + c, S3 = MidState[3] + d;
        uint32_t S4 = MidState[4] + e, S5 = MidState[5] + f;
        uint32_t S6 = MidState[6] + g, S7 = MidState[7] + h;

        uint32_t W2_16 = SIG0(S1) + S0;
        uint32_t W2_17 = W2_SIG1_15 + SIG0(S2) + S1;
        uint32_t W2_18 = SIG1(W2_16) + SIG0(S3) + S2;
        uint32_t W2_19 = SIG1(W2_17) + SIG0(S4) + S3;
        uint32_t W2_20 = SIG1(W2_18) + SIG0(S5) + S4;
        uint32_t W2_21 = SIG1(W2_19) + SIG0(S6) + S5;
        uint32_t W2_22 = SIG1(W2_20) + W2_15 + SIG0(S7) + S6;
        uint32_t W2_23 = SIG1(W2_21) + W2_16 + W2_SIG0_8 + S7;
        uint32_t W2_24 = SIG1(W2_22) + W2_17 + W2_8;
        uint32_t W2_25 = SIG1(W2_23) + W2_18;
        uint32_t W2_26 = SIG1(W2_24) + W2_19;
        uint32_t W2_27 = SIG1(W2_25) + W2_20;
        uint32_t W2_28 = SIG1(W2_26) + W2_21;
        uint32_t W2_29 = SIG1(W2_27) + W2_22;
        uint32_t W2_30 = SIG1(W2_28) + W2_23 + SIG0(W2_15);
        uint32_t W2_31 = SIG1(W2_29) + W2_24 + SIG0(W2_16) + W2_15;
        uint32_t W2_32 = SIG1(W2_30) + W2_25 + SIG0(W2_17) + W2_16;
        uint32_t W2_33 = SIG1(W2_31) + W2_26 + SIG0(W2_18) + W2_17;
        uint32_t W2_34 = SIG1(W2_32) + W2_27 + SIG0(W2_19) + W2_18;
        uint32_t W2_35 = SIG1(W2_33) + W2_28 + SIG0(W2_20) + W2_19;
        uint32_t W2_36 = SIG1(W2_34) + W2_29 + SIG0(W2_21) + W2_20;
        uint32_t W2_37 = SIG1(W2_35) + W2_30 + SIG0(W2_22) + W2_21;
        uint32_t W2_38 = SIG1(W2_36) + W2_31 + SIG0(W2_23) + W2_22;
        uint32_t W2_39 = SIG1(W2_37) + W2_32 + SIG0(W2_24) + W2_23;
        uint32_t W2_40 = SIG1(W2_38) + W2_33 + SIG0(W2_25) + W2_24;
        uint32_t W2_41 = SIG1(W2_39) + W2_34 + SIG0(W2_26) + W2_25;
        uint32_t W2_42 = SIG1(W2_40) + W2_35 + SIG0(W2_27) + W2_26;
        uint32_t W2_43 = SIG1(W2_41) + W2_36 + SIG0(W2_28) + W2_27;
        uint32_t W2_44 = SIG1(W2_42) + W2_37 + SIG0(W2_29) + W2_28;
        uint32_t W2_45 = SIG1(W2_43) + W2_38 + SIG0(W2_30) + W2_29;
        uint32_t W2_46 = SIG1(W2_44) + W2_39 + SIG0(W2_31) + W2_30;
        uint32_t W2_47 = SIG1(W2_45) + W2_40 + SIG0(W2_32) + W2_31;
        uint32_t W2_48 = SIG1(W2_46) + W2_41 + SIG0(W2_33) + W2_32;
        uint32_t W2_49 = SIG1(W2_47) + W2_42 + SIG0(W2_34) + W2_33;
        uint32_t W2_50 = SIG1(W2_48) + W2_43 + SIG0(W2_35) + W2_34;
        uint32_t W2_51 = SIG1(W2_49) + W2_44 + SIG0(W2_36) + W2_35;
        uint32_t W2_52 = SIG1(W2_50) + W2_45 + SIG0(W2_37) + W2_36;
        uint32_t W2_53 = SIG1(W2_51) + W2_46 + SIG0(W2_38) + W2_37;
        uint32_t W2_54 = SIG1(W2_52) + W2_47 + SIG0(W2_39) + W2_38;
        uint32_t W2_55 = SIG1(W2_53) + W2_48 + SIG0(W2_40) + W2_39;
        uint32_t W2_56 = SIG1(W2_54) + W2_49 + SIG0(W2_41) + W2_40;
        uint32_t W2_57 = SIG1(W2_55) + W2_50 + SIG0(W2_42) + W2_41;
        uint32_t W2_58 = SIG1(W2_56) + W2_51 + SIG0(W2_43) + W2_42;
        uint32_t W2_59 = SIG1(W2_57) + W2_52 + SIG0(W2_44) + W2_43;
        uint32_t W2_60 = SIG1(W2_58) + W2_53 + SIG0(W2_45) + W2_44;
        uint32_t W2_61 = SIG1(W2_59) + W2_54 + SIG0(W2_46) + W2_45;
        uint32_t W2_62 = SIG1(W2_60) + W2_55 + SIG0(W2_47) + W2_46;
        uint32_t W2_63 = SIG1(W2_61) + W2_56 + SIG0(W2_48) + W2_47;

        a = H_INIT[0]; b = H_INIT[1]; c = H_INIT[2]; d = H_INIT[3];
        e = H_INIT[4]; f = H_INIT[5]; g = H_INIT[6]; h = H_INIT[7];

        SHA256_ROUND(a,b,c,d,e,f,g,h, K[0], S0);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[1], S1);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[2], S2);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[3], S3);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[4], S4);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[5], S5);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[6], S6);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[7], S7);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_8);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_9);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_10);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_11);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_12);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_13);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_14);
        SHA256_ROUND_KW(a,b,c,d,e,f,g,h, K2_15);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[16], W2_16);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[17], W2_17);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[18], W2_18);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[19], W2_19);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[20], W2_20);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[21], W2_21);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[22], W2_22);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[23], W2_23);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[24], W2_24);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[25], W2_25);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[26], W2_26);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[27], W2_27);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[28], W2_28);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[29], W2_29);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[30], W2_30);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[31], W2_31);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[32], W2_32);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[33], W2_33);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[34], W2_34);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[35], W2_35);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[36], W2_36);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[37], W2_37);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[38], W2_38);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[39], W2_39);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[40], W2_40);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[41], W2_41);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[42], W2_42);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[43], W2_43);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[44], W2_44);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[45], W2_45);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[46], W2_46);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[47], W2_47);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[48], W2_48);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[49], W2_49);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[50], W2_50);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[51], W2_51);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[52], W2_52);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[53], W2_53);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[54], W2_54);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[55], W2_55);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[56], W2_56);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[57], W2_57);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[58], W2_58);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[59], W2_59);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[60], W2_60);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[61], W2_61);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[62], W2_62);
        SHA256_ROUND(a,b,c,d,e,f,g,h, K[63], W2_63);

        uint32_t FinalState0 = H_INIT[0] + a;

        if (FinalState0 <= TargetHigh) {
            uint32_t FinalState[8];
            FinalState[0] = FinalState0;
            FinalState[1] = H_INIT[1] + b;
            FinalState[2] = H_INIT[2] + c;
            FinalState[3] = H_INIT[3] + d;
            FinalState[4] = H_INIT[4] + e;
            FinalState[5] = H_INIT[5] + f;
            FinalState[6] = H_INIT[6] + g;
            FinalState[7] = H_INIT[7] + h;
            if (CheckTarget(FinalState, p_Job->Target)) {
                *p_Nonce = Nonce;
                *p_Found = true;
                return PdqOk;
            }
        }
    }

    return PdqOk;
}
