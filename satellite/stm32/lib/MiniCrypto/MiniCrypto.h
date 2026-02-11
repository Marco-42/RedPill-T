#ifndef MINICRYPTO_H
#define MINICRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class SHA256 {
public:
    SHA256();
    void reset();
    void update(const void *data, size_t len);
    void finalize(void *hash, size_t len);

    // HMAC methods
    void resetHMAC(const void *key, size_t keyLen);
    void finalizeHMAC(const void *key, size_t keyLen, void *hash, size_t hashLen);

private:
    uint32_t state[8];
    uint32_t count[2];
    uint8_t buffer[64];
    uint8_t i_pad[64];
    uint8_t o_pad[64];
    void process();
};

#endif