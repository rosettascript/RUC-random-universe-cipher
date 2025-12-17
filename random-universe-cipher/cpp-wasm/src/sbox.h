#ifndef SBOX_H
#define SBOX_H

#include <cstdint>
#include <cstddef>

// Generate S-box for a specific round
void generate_sbox(const uint8_t* key, size_t key_len, uint16_t round, uint8_t* sbox);

#endif // SBOX_H

