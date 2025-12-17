#ifndef RUC_CIPHER_H
#define RUC_CIPHER_H

#include <cstdint>
#include <cstddef>

// Constants matching TypeScript implementation
constexpr size_t KEY_SIZE = 64;           // 512 bits
constexpr size_t BLOCK_SIZE = 32;          // 256 bits
constexpr size_t REGISTER_SIZE = 64;       // 512 bits
constexpr size_t ACCUMULATOR_SIZE = 128;    // 1024 bits
constexpr size_t IV_SIZE = 32;             // 256 bits
constexpr size_t NONCE_SIZE = 16;           // 128 bits
constexpr size_t ROUNDS = 24;
constexpr size_t REGISTER_COUNT = 7;
constexpr size_t MIN_SELECTORS = 16;
constexpr size_t MAX_SELECTORS = 31;
constexpr uint8_t GF_POLYNOMIAL = 0x1B;

// Cipher state structure
struct CipherState {
    uint8_t registers[REGISTER_COUNT][REGISTER_SIZE];
    uint8_t accumulator[ACCUMULATOR_SIZE];
};

// Key material structure
struct KeyMaterial {
    uint8_t registers[REGISTER_COUNT][REGISTER_SIZE];
    uint16_t selectors[MAX_SELECTORS];
    size_t num_selectors;
    uint8_t round_keys[ROUNDS][REGISTER_SIZE];
    uint8_t sboxes[ROUNDS][256];
    uint8_t key_constants[MAX_SELECTORS]; // Pre-computed constants for selectors (indexed by selector position)
};

// External C interface for WASM
extern "C" {
    // Initialize key material from key
    void* ruc_expand_key(const uint8_t* key);
    
    // Free key material
    void ruc_free_key_material(void* km);
    
    // Encrypt a single block
    void ruc_encrypt_block(
        const uint8_t* plaintext,
        const uint8_t* key,
        const uint8_t* iv,
        uint32_t block_number,
        void* key_material,
        uint8_t* ciphertext
    );
    
    // Decrypt a single block (same as encrypt for XOR cipher)
    void ruc_decrypt_block(
        const uint8_t* ciphertext,
        const uint8_t* key,
        const uint8_t* iv,
        uint32_t block_number,
        void* key_material,
        uint8_t* plaintext
    );
    
    // Encrypt multiple blocks in parallel (for worker processing)
    // Using uint32_t instead of uint64_t to avoid BigInt conversion issues in Emscripten
    // This is safe: 2^32 blocks = 128GB of data, which is sufficient for most use cases
    // Optimized version that caches IV expansion and other expensive operations
    void ruc_encrypt_blocks_batch(
        const uint8_t* plaintext_blocks,
        size_t num_blocks,
        const uint8_t* key,
        const uint8_t* iv,
        uint32_t start_block_number,
        void* key_material,
        uint8_t* ciphertext_blocks
    );
    
    // Decrypt multiple blocks in parallel
    void ruc_decrypt_blocks_batch(
        const uint8_t* ciphertext_blocks,
        size_t num_blocks,
        const uint8_t* key,
        const uint8_t* iv,
        uint32_t start_block_number,
        void* key_material,
        uint8_t* plaintext_blocks
    );
    
    // Profiling: Get performance counters (for debugging)
    void ruc_get_profile_stats(
        uint64_t* shake256_calls,
        uint64_t* shake256_time_us,
        uint64_t* rounds_time_us,
        uint64_t* selector_ordering_time_us,
        uint64_t* keystream_time_us,
        uint64_t* counter_hash_time_us,
        uint64_t* gf_mul_calls,
        uint64_t* register_ops_calls
    );
}

#endif // RUC_CIPHER_H

