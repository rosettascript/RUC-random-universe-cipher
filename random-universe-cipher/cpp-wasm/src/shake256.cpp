#include "shake256.h"
#include <cstring>
#include <vector>

// Simple SHAKE256 implementation using Keccak-f[1600]
// For production, use a proper SHAKE256 library like tiny_sha3

// Keccak-f[1600] permutation constants
static const uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const int ROT[5][5] = {
    {0, 36, 3, 41, 18}, {1, 44, 10, 45, 2}, {62, 6, 43, 15, 61},
    {28, 55, 25, 21, 56}, {27, 20, 39, 8, 14}
};

// Rotate left (inline for better performance - called thousands of times)
static inline uint64_t rotl64(uint64_t x, int y) {
    return (x << y) | (x >> (64 - y));
}

// Macro to generate a single Keccak round (fully unrolled for maximum performance)
#define KECCAK_ROUND(round_num) \
    do { \
        /* Theta */ \
        uint64_t C0 = state[0] ^ state[5] ^ state[10] ^ state[15] ^ state[20]; \
        uint64_t C1 = state[1] ^ state[6] ^ state[11] ^ state[16] ^ state[21]; \
        uint64_t C2 = state[2] ^ state[7] ^ state[12] ^ state[17] ^ state[22]; \
        uint64_t C3 = state[3] ^ state[8] ^ state[13] ^ state[18] ^ state[23]; \
        uint64_t C4 = state[4] ^ state[9] ^ state[14] ^ state[19] ^ state[24]; \
        uint64_t D0 = C4 ^ rotl64(C1, 1); \
        uint64_t D1 = C0 ^ rotl64(C2, 1); \
        uint64_t D2 = C1 ^ rotl64(C3, 1); \
        uint64_t D3 = C2 ^ rotl64(C4, 1); \
        uint64_t D4 = C3 ^ rotl64(C0, 1); \
        state[0] ^= D0; state[5] ^= D0; state[10] ^= D0; state[15] ^= D0; state[20] ^= D0; \
        state[1] ^= D1; state[6] ^= D1; state[11] ^= D1; state[16] ^= D1; state[21] ^= D1; \
        state[2] ^= D2; state[7] ^= D2; state[12] ^= D2; state[17] ^= D2; state[22] ^= D2; \
        state[3] ^= D3; state[8] ^= D3; state[13] ^= D3; state[18] ^= D3; state[23] ^= D3; \
        state[4] ^= D4; state[9] ^= D4; state[14] ^= D4; state[19] ^= D4; state[24] ^= D4; \
        /* Rho and Pi */ \
        uint64_t B[25]; \
        B[0] = rotl64(state[0], 0); B[1] = rotl64(state[15], 28); B[2] = rotl64(state[5], 1); \
        B[3] = rotl64(state[20], 27); B[4] = rotl64(state[10], 62); B[5] = rotl64(state[6], 44); \
        B[6] = rotl64(state[21], 20); B[7] = rotl64(state[11], 6); B[8] = rotl64(state[1], 36); \
        B[9] = rotl64(state[16], 55); B[10] = rotl64(state[12], 43); B[11] = rotl64(state[2], 3); \
        B[12] = rotl64(state[17], 25); B[13] = rotl64(state[7], 10); B[14] = rotl64(state[22], 39); \
        B[15] = rotl64(state[18], 21); B[16] = rotl64(state[8], 45); B[17] = rotl64(state[23], 8); \
        B[18] = rotl64(state[13], 15); B[19] = rotl64(state[3], 41); B[20] = rotl64(state[24], 18); \
        B[21] = rotl64(state[14], 2); B[22] = rotl64(state[9], 61); B[23] = rotl64(state[4], 56); \
        B[24] = rotl64(state[19], 14); \
        /* Chi */ \
        state[0] = B[0] ^ ((~B[1]) & B[2]); state[1] = B[1] ^ ((~B[2]) & B[3]); \
        state[2] = B[2] ^ ((~B[3]) & B[4]); state[3] = B[3] ^ ((~B[4]) & B[0]); \
        state[4] = B[4] ^ ((~B[0]) & B[1]); state[5] = B[5] ^ ((~B[6]) & B[7]); \
        state[6] = B[6] ^ ((~B[7]) & B[8]); state[7] = B[7] ^ ((~B[8]) & B[9]); \
        state[8] = B[8] ^ ((~B[9]) & B[5]); state[9] = B[9] ^ ((~B[5]) & B[6]); \
        state[10] = B[10] ^ ((~B[11]) & B[12]); state[11] = B[11] ^ ((~B[12]) & B[13]); \
        state[12] = B[12] ^ ((~B[13]) & B[14]); state[13] = B[13] ^ ((~B[14]) & B[10]); \
        state[14] = B[14] ^ ((~B[10]) & B[11]); state[15] = B[15] ^ ((~B[16]) & B[17]); \
        state[16] = B[16] ^ ((~B[17]) & B[18]); state[17] = B[17] ^ ((~B[18]) & B[19]); \
        state[18] = B[18] ^ ((~B[19]) & B[15]); state[19] = B[19] ^ ((~B[15]) & B[16]); \
        state[20] = B[20] ^ ((~B[21]) & B[22]); state[21] = B[21] ^ ((~B[22]) & B[23]); \
        state[22] = B[22] ^ ((~B[23]) & B[24]); state[23] = B[23] ^ ((~B[24]) & B[20]); \
        state[24] = B[24] ^ ((~B[20]) & B[21]); \
        /* Iota */ \
        state[0] ^= RC[round_num]; \
    } while(0)

// Keccak-f[1600] permutation (FULLY UNROLLED - all 24 rounds inline for maximum performance)
static void keccak_f(uint64_t state[25]) {
    // Fully unroll all 24 rounds - eliminates loop overhead and enables better compiler optimization
    KECCAK_ROUND(0); KECCAK_ROUND(1); KECCAK_ROUND(2); KECCAK_ROUND(3);
    KECCAK_ROUND(4); KECCAK_ROUND(5); KECCAK_ROUND(6); KECCAK_ROUND(7);
    KECCAK_ROUND(8); KECCAK_ROUND(9); KECCAK_ROUND(10); KECCAK_ROUND(11);
    KECCAK_ROUND(12); KECCAK_ROUND(13); KECCAK_ROUND(14); KECCAK_ROUND(15);
    KECCAK_ROUND(16); KECCAK_ROUND(17); KECCAK_ROUND(18); KECCAK_ROUND(19);
    KECCAK_ROUND(20); KECCAK_ROUND(21); KECCAK_ROUND(22); KECCAK_ROUND(23);
}

// External counter for profiling (defined in ruc_cipher.cpp)
extern uint64_t profile_shake256_calls;

// SHAKE256 sponge function (optimized)
void shake256_hash(const uint8_t* input, size_t input_len, uint8_t* output, size_t output_len) {
    profile_shake256_calls++;
    uint64_t state[25] = {0};
    const size_t rate = 136; // SHAKE256 rate in bytes (1088 bits = 136 bytes)
    
    // Absorb phase - optimized for small inputs (most common case)
    size_t pos = 0;
    if (input_len <= rate) {
        // Fast path: input fits in one rate block
        for (size_t i = 0; i < input_len; i++) {
            size_t lane = i / 8;
            size_t byte_in_lane = i % 8;
            state[lane] ^= ((uint64_t)input[i]) << (byte_in_lane * 8);
        }
        pos = input_len;
    } else {
        // General case: multiple rate blocks
        for (size_t i = 0; i < input_len; i++) {
            size_t lane = pos / 8;
            size_t byte_in_lane = pos % 8;
            state[lane] ^= ((uint64_t)input[i]) << (byte_in_lane * 8);
            pos++;
            if (pos == rate) {
                keccak_f(state);
                pos = 0;
            }
        }
    }
    
    // Padding for SHAKE256: domain separator 0x1F
    size_t lane = pos / 8;
    size_t byte_in_lane = pos % 8;
    state[lane] ^= ((uint64_t)0x1F) << (byte_in_lane * 8);
    
    // Set the last bit (0x80) in the last byte of the rate block
    size_t last_lane = (rate - 1) / 8;
    size_t last_byte_in_lane = (rate - 1) % 8;
    state[last_lane] ^= ((uint64_t)0x80) << (last_byte_in_lane * 8);
    
    keccak_f(state);
    
    // Squeeze phase - optimized for common output sizes (32 bytes is most common)
    pos = 0;
    if (output_len == 32) {
        // Fast path: 32-byte output (most common case for BLOCK_SIZE)
        // Output fits in first 4 lanes (32 bytes) - no need for keccak_f
        for (int i = 0; i < 4; i++) {
            uint64_t lane_val = state[i];
            output[i*8 + 0] = (uint8_t)(lane_val & 0xFF);
            output[i*8 + 1] = (uint8_t)((lane_val >> 8) & 0xFF);
            output[i*8 + 2] = (uint8_t)((lane_val >> 16) & 0xFF);
            output[i*8 + 3] = (uint8_t)((lane_val >> 24) & 0xFF);
            output[i*8 + 4] = (uint8_t)((lane_val >> 32) & 0xFF);
            output[i*8 + 5] = (uint8_t)((lane_val >> 40) & 0xFF);
            output[i*8 + 6] = (uint8_t)((lane_val >> 48) & 0xFF);
            output[i*8 + 7] = (uint8_t)((lane_val >> 56) & 0xFF);
        }
    } else {
        // General case
        for (size_t i = 0; i < output_len; i++) {
            if (pos == rate) {
                keccak_f(state);
                pos = 0;
            }
            size_t lane = pos / 8;
            size_t byte_in_lane = pos % 8;
            output[i] = (uint8_t)((state[lane] >> (byte_in_lane * 8)) & 0xFF);
            pos++;
        }
    }
}

// SHAKE256 with domain separation
void shake256_with_domain(
    const uint8_t* key,
    size_t key_len,
    const char* domain,
    uint16_t index,
    uint8_t* output,
    size_t output_len
) {
    std::vector<uint8_t> input;
    input.insert(input.end(), key, key + key_len);
    
    size_t domain_len = strlen(domain);
    input.insert(input.end(), domain, domain + domain_len);
    
    input.push_back((index >> 8) & 0xFF);
    input.push_back(index & 0xFF);
    
    shake256_hash(input.data(), input.size(), output, output_len);
}

