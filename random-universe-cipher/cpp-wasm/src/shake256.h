#ifndef SHAKE256_H
#define SHAKE256_H

#include <cstdint>
#include <cstddef>

// SHAKE256 hash function
void shake256_hash(const uint8_t* input, size_t input_len, uint8_t* output, size_t output_len);

// SHAKE256 with domain separation
void shake256_with_domain(
    const uint8_t* key,
    size_t key_len,
    const char* domain,
    uint16_t index,
    uint8_t* output,
    size_t output_len
);

#endif // SHAKE256_H

