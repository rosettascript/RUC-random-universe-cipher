#include "sbox.h"
#include "shake256.h"
#include <cstring>

void generate_sbox(const uint8_t* key, size_t key_len, uint16_t round, uint8_t* sbox) {
    // Initialize identity permutation
    for (int i = 0; i < 256; i++) {
        sbox[i] = i;
    }
    
    // Generate shuffle seed
    uint8_t shuffle_seed[512];
    uint8_t round_bytes[2] = {(uint8_t)(round >> 8), (uint8_t)(round & 0xFF)};
    
    // Concatenate key || "RUC-SBOX" || round_bytes
    uint8_t domain[] = "RUC-SBOX";
    size_t input_len = key_len + 8 + 2;
    uint8_t* input = new uint8_t[input_len];
    memcpy(input, key, key_len);
    memcpy(input + key_len, domain, 8);
    memcpy(input + key_len + 8, round_bytes, 2);
    
    shake256_hash(input, input_len, shuffle_seed, 512);
    delete[] input;
    
    // Fisher-Yates shuffle
    for (int i = 255; i > 0; i--) {
        int idx = 2 * (255 - i);
        uint16_t rand_val = (shuffle_seed[idx] << 8) | shuffle_seed[idx + 1];
        int j = rand_val % (i + 1);
        
        // Swap
        uint8_t temp = sbox[i];
        sbox[i] = sbox[j];
        sbox[j] = temp;
    }
}

