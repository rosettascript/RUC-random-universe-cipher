#include "chacha20.h"
#include <cstring>

static const uint32_t CHACHA_CONSTANTS[4] = {
    0x61707865, 0x3320646e, 0x79622d32, 0x6b206574
};

static uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static void quarter_round(uint32_t* state, int a, int b, int c, int d) {
    state[a] = (state[a] + state[b]);
    state[d] = rotl32(state[d] ^ state[a], 16);
    
    state[c] = (state[c] + state[d]);
    state[b] = rotl32(state[b] ^ state[c], 12);
    
    state[a] = (state[a] + state[b]);
    state[d] = rotl32(state[d] ^ state[a], 8);
    
    state[c] = (state[c] + state[d]);
    state[b] = rotl32(state[b] ^ state[c], 7);
}

static void chacha_block(const uint8_t* key, const uint8_t* nonce, uint32_t counter, uint8_t* output) {
    uint32_t state[16];
    
    // Constants
    memcpy(state, CHACHA_CONSTANTS, 16);
    
    // Key (little-endian)
    for (int i = 0; i < 8; i++) {
        state[4 + i] = 
            ((uint32_t)key[i * 4]) |
            ((uint32_t)key[i * 4 + 1] << 8) |
            ((uint32_t)key[i * 4 + 2] << 16) |
            ((uint32_t)key[i * 4 + 3] << 24);
    }
    
    // Counter
    state[12] = counter;
    
    // Nonce (little-endian)
    for (int i = 0; i < 3; i++) {
        state[13 + i] = 
            ((uint32_t)nonce[i * 4]) |
            ((uint32_t)nonce[i * 4 + 1] << 8) |
            ((uint32_t)nonce[i * 4 + 2] << 16) |
            ((uint32_t)nonce[i * 4 + 3] << 24);
    }
    
    uint32_t working[16];
    memcpy(working, state, 64);
    
    // 20 rounds (10 double rounds)
    for (int i = 0; i < 10; i++) {
        quarter_round(working, 0, 4, 8, 12);
        quarter_round(working, 1, 5, 9, 13);
        quarter_round(working, 2, 6, 10, 14);
        quarter_round(working, 3, 7, 11, 15);
        
        quarter_round(working, 0, 5, 10, 15);
        quarter_round(working, 1, 6, 11, 12);
        quarter_round(working, 2, 7, 8, 13);
        quarter_round(working, 3, 4, 9, 14);
    }
    
    for (int i = 0; i < 16; i++) {
        working[i] += state[i];
    }
    
    memcpy(output, working, 64);
}

ChaCha20PRNG::ChaCha20PRNG(const uint8_t* seed_key, const uint8_t* seed_nonce) {
    memcpy(key, seed_key, 32);
    if (seed_nonce) {
        memcpy(nonce, seed_nonce, 12);
    } else {
        memset(nonce, 0, 12);
    }
    counter = 0;
    buffer_pos = 64;
}

void ChaCha20PRNG::generate_block() {
    chacha_block(key, nonce, counter, buffer);
    counter++;
    buffer_pos = 0;
}

void ChaCha20PRNG::next_bytes(uint8_t* output, size_t count) {
    size_t written = 0;
    while (written < count) {
        if (buffer_pos >= 64) {
            generate_block();
        }
        size_t available = 64 - buffer_pos;
        size_t to_write = (count - written < available) ? (count - written) : available;
        memcpy(output + written, buffer + buffer_pos, to_write);
        buffer_pos += to_write;
        written += to_write;
    }
}

uint32_t ChaCha20PRNG::next_u32() {
    uint8_t bytes[4];
    next_bytes(bytes, 4);
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

uint32_t ChaCha20PRNG::next_int(uint32_t max) {
    uint32_t max_valid = (0xFFFFFFFFU / max) * max;
    uint32_t value;
    do {
        value = next_u32();
    } while (value >= max_valid);
    return value % max;
}

void ChaCha20PRNG::reset() {
    counter = 0;
    buffer_pos = 64;
}

