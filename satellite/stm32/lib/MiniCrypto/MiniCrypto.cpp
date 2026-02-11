#include "MiniCrypto.h"

#define SHA2_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA2_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA2_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA2_SIGMA0(x) (SHA2_ROTR(x, 2) ^ SHA2_ROTR(x, 13) ^ SHA2_ROTR(x, 22))
#define SHA2_SIGMA1(x) (SHA2_ROTR(x, 6) ^ SHA2_ROTR(x, 11) ^ SHA2_ROTR(x, 25))
#define SHA2_sigma0(x) (SHA2_ROTR(x, 7) ^ SHA2_ROTR(x, 18) ^ ((x) >> 3))
#define SHA2_sigma1(x) (SHA2_ROTR(x, 17) ^ SHA2_ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

SHA256::SHA256() { reset(); }

void SHA256::reset() {
    state[0] = 0x6a09e667; state[1] = 0xbb67ae85; state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
    state[4] = 0x510e527f; state[5] = 0x9b05688c; state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
    count[0] = count[1] = 0;
}

void SHA256::process() {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    size_t i;
    for (i = 0; i < 16; i++) {
        w[i] = (buffer[i * 4] << 24) | (buffer[i * 4 + 1] << 16) | (buffer[i * 4 + 2] << 8) | buffer[i * 4 + 3];
    }
    for (; i < 64; i++) {
        w[i] = SHA2_sigma1(w[i - 2]) + w[i - 7] + SHA2_sigma0(w[i - 15]) + w[i - 16];
    }
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + SHA2_SIGMA1(e) + SHA2_CH(e, f, g) + k[i] + w[i];
        uint32_t t2 = SHA2_SIGMA0(a) + SHA2_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void SHA256::update(const void *data, size_t len) {
    const uint8_t *d = (const uint8_t *)data;
    uint32_t i, index, partLen;
    index = (uint32_t)((count[0] >> 3) & 0x3F);
    partLen = 64 - index;
    if ((count[0] += ((uint32_t)len << 3)) < ((uint32_t)len << 3)) count[1]++;
    count[1] += ((uint32_t)len >> 29);
    if (len >= partLen) {
        memcpy(&buffer[index], d, partLen);
        process();
        for (i = partLen; i + 63 < len; i += 64) {
            memcpy(buffer, &d[i], 64);
            process();
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&buffer[index], &d[i], len - i);
}

void SHA256::finalize(void *hash, size_t len) {
    uint8_t t[8];
    uint32_t i, index, padLen;
    t[0] = (uint8_t)(count[1] >> 24); t[1] = (uint8_t)(count[1] >> 16);
    t[2] = (uint8_t)(count[1] >> 8);  t[3] = (uint8_t)count[1];
    t[4] = (uint8_t)(count[0] >> 24); t[5] = (uint8_t)(count[0] >> 16);
    t[6] = (uint8_t)(count[0] >> 8);  t[7] = (uint8_t)count[0];
    index = (uint32_t)((count[0] >> 3) & 0x3F);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update("\x80", 1);
    while (padLen > 0) { update("\x00", 1); padLen--; }
    update(t, 8);
    for (i = 0; i < 32 && i < len; i++) {
        ((uint8_t *)hash)[i] = (uint8_t)(state[i >> 2] >> ((3 - (i & 3)) * 8));
    }
}

void SHA256::resetHMAC(const void *key, size_t keyLen) {
    reset();
    memset(i_pad, 0x36, 64);
    memset(o_pad, 0x5c, 64);
    uint8_t k[64] = {0};
    if (keyLen > 64) {
        SHA256 k_sha;
        k_sha.update(key, keyLen);
        k_sha.finalize(k, 32);
    } else {
        memcpy(k, key, keyLen);
    }
    for (int i = 0; i < 64; i++) {
        i_pad[i] ^= k[i];
        o_pad[i] ^= k[i];
    }
    update(i_pad, 64);
}

void SHA256::finalizeHMAC(const void *key, size_t keyLen, void *hash, size_t hashLen) {
    uint8_t innerHash[32];
    finalize(innerHash, 32);
    reset();
    update(o_pad, 64);
    update(innerHash, 32);
    finalize(hash, hashLen);
}