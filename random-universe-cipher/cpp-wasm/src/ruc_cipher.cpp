#include "ruc_cipher.h"
#include "gf_math.h"
#include "shake256.h"
#include "chacha20.h"
#include "sbox.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cstdio>

// Profiling counters (for performance analysis) - exported for access from shake256.cpp
uint64_t profile_shake256_calls = 0;
static uint64_t profile_rounds_executed = 0;
static uint64_t profile_selector_ordering_calls = 0;
static uint64_t profile_keystream_calls = 0;
static uint64_t profile_counter_hash_calls = 0;
static uint64_t profile_gf_mul_calls = 0;
static uint64_t profile_register_ops_calls = 0;
static uint64_t profile_blocks_processed = 0;

// Rotate 512-bit register left by n bits
static void rotate_left_512(const uint8_t* reg, int n, uint8_t* result) {
    n = n % 512;
    if (n == 0) {
        memcpy(result, reg, REGISTER_SIZE);
        return;
    }
    
    int byte_shift = n / 8;
    int bit_shift = n % 8;
    
    for (int i = 0; i < REGISTER_SIZE; i++) {
        int src_idx = (i + byte_shift) % REGISTER_SIZE;
        int next_idx = (src_idx + 1) % REGISTER_SIZE;
        
        uint8_t low = (reg[src_idx] << bit_shift) & 0xFF;
        uint8_t high = (bit_shift > 0) ? (reg[next_idx] >> (8 - bit_shift)) : 0;
        result[i] = low | high;
    }
}

// XOR two 512-bit registers (in-place version)
static void xor_512_inplace(uint8_t* a, const uint8_t* b) {
    // Use 64-bit operations for better performance (8 bytes at a time)
    uint64_t* a64 = (uint64_t*)a;
    const uint64_t* b64 = (const uint64_t*)b;
    for (int i = 0; i < REGISTER_SIZE / 8; i++) {
        a64[i] ^= b64[i];
    }
}

// XOR two 512-bit registers (out-of-place version, kept for compatibility)
static void xor_512(const uint8_t* a, const uint8_t* b, uint8_t* result) {
    const uint64_t* a64 = (const uint64_t*)a;
    const uint64_t* b64 = (const uint64_t*)b;
    uint64_t* r64 = (uint64_t*)result;
    for (int i = 0; i < REGISTER_SIZE / 8; i++) {
        r64[i] = a64[i] ^ b64[i];
    }
}

// Convert 64-byte register to uint64_t (little-endian, first 8 bytes) - optimized
static uint64_t bytes_to_u64(const uint8_t* bytes) {
    // Direct memory access (faster than loop)
    return ((uint64_t)bytes[0]) |
           ((uint64_t)bytes[1] << 8) |
           ((uint64_t)bytes[2] << 16) |
           ((uint64_t)bytes[3] << 24) |
           ((uint64_t)bytes[4] << 32) |
           ((uint64_t)bytes[5] << 40) |
           ((uint64_t)bytes[6] << 48) |
           ((uint64_t)bytes[7] << 56);
}

// Order selectors by priority
static void order_selectors(
    const KeyMaterial* km,
    const uint8_t* key,
    const uint8_t* iv,
    uint64_t block_number,
    uint16_t* ordered_selectors,
    size_t* selector_indices  // Output: index in km->selectors for each ordered selector
) {
    // Create seed: key || iv || block_number || "RUC-PRIO"
    uint8_t block_bytes[8];
    for (int i = 0; i < 8; i++) {
        block_bytes[i] = (block_number >> (i * 8)) & 0xFF;
    }
    
    uint8_t domain[] = "RUC-PRIO";
    size_t seed_len = KEY_SIZE + IV_SIZE + 8 + 8;
    uint8_t* seed_input = new uint8_t[seed_len];
    memcpy(seed_input, key, KEY_SIZE);
    memcpy(seed_input + KEY_SIZE, iv, IV_SIZE);
    memcpy(seed_input + KEY_SIZE + IV_SIZE, block_bytes, 8);
    memcpy(seed_input + KEY_SIZE + IV_SIZE + 8, domain, 8);
    
    uint8_t seed[32];
    shake256_hash(seed_input, seed_len, seed, 32);
    delete[] seed_input;
    
    ChaCha20PRNG prng(seed);
    
    // Assign priorities
    struct PriorityItem {
        uint16_t selector;
        uint32_t priority;
        size_t index;
    };
    
    std::vector<PriorityItem> priorities;
    for (size_t i = 0; i < km->num_selectors; i++) {
        PriorityItem item;
        item.selector = km->selectors[i];
        item.priority = prng.next_int(7);
        item.index = i;
        priorities.push_back(item);
    }
    
    // Stable sort by priority - use insertion sort for small arrays (~20-30 elements)
    // This is faster than std::stable_sort for small arrays due to lower overhead
    for (size_t i = 1; i < priorities.size(); i++) {
        PriorityItem key = priorities[i];
        int j = i - 1;
        while (j >= 0 && (priorities[j].priority > key.priority || 
               (priorities[j].priority == key.priority && priorities[j].index > key.index))) {
            priorities[j + 1] = priorities[j];
            j--;
        }
        priorities[j + 1] = key;
    }
    
    for (size_t i = 0; i < km->num_selectors; i++) {
        ordered_selectors[i] = priorities[i].selector;
        selector_indices[i] = priorities[i].index;  // Store index for fast lookup
    }
}

// Execute a single round
static void execute_round(
    CipherState* state,
    int round_index,
    const uint16_t* ordered_selectors,
    const size_t* selector_indices,  // New: pre-computed indices for fast constant lookup
    size_t num_selectors,
    const KeyMaterial* km,
    const uint8_t* key
) {
    const uint8_t* sbox = km->sboxes[round_index];
    const uint8_t* round_key = km->round_keys[round_index];
    
    // Process each selector
    for (size_t sel_idx = 0; sel_idx < num_selectors; sel_idx++) {
        uint16_t sel = ordered_selectors[sel_idx];
        
        // Select destination register: (R[0] XOR selector XOR roundKey) mod 7
        uint64_t r0_u64 = bytes_to_u64(state->registers[0]);
        uint64_t round_key_u64 = bytes_to_u64(round_key);
        uint64_t dest_val = (r0_u64 ^ (uint64_t)sel ^ round_key_u64) & 0xFFFFFFFFULL;
        size_t place_idx = dest_val % 7;
        
        // Compute non-linear transformation
        uint16_t temp = (sel * 2) & 0xFFFF;
        uint8_t state_byte = state->registers[place_idx][0]; // Top byte
        
        // GF multiplication
        profile_gf_mul_calls++;
        uint8_t gf_result = gf_mul(temp & 0xFF, state_byte);
        
        // XOR with pre-computed key constant (optimized: direct lookup using pre-computed index)
        uint8_t key_const = km->key_constants[selector_indices[sel_idx]];
        gf_result ^= key_const;
        
        // Apply S-box
        uint8_t result = sbox[gf_result];
        
        // Update state register: GF multiply each byte (in-place)
        uint8_t* reg = state->registers[place_idx];
        profile_gf_mul_calls += REGISTER_SIZE;
        gf_mul_register_inplace(reg, result);
        
        // XOR with shifted result (in-place)
        int shift_amount = sel % 16;
        if (shift_amount < 8) {
            reg[0] ^= (result << shift_amount);
        }
        
        // Apply S-box to low byte (in-place)
        reg[REGISTER_SIZE - 1] ^= sbox[reg[REGISTER_SIZE - 1]];
        
        // Rotate left by 1 (optimized for n=1)
        uint8_t first_byte = reg[0];
        for (int i = 0; i < REGISTER_SIZE - 1; i++) {
            reg[i] = (reg[i] >> 1) | (reg[i + 1] << 7);
        }
        reg[REGISTER_SIZE - 1] = (reg[REGISTER_SIZE - 1] >> 1) | (first_byte << 7);
        
        // Mix with adjacent register (in-place)
        profile_register_ops_calls++;
        xor_512_inplace(reg, state->registers[(place_idx + 1) % REGISTER_COUNT]);
        
        // Accumulate result (simplified - track sum)
        uint64_t* acc_sum = (uint64_t*)state->accumulator;
        *acc_sum = (*acc_sum + result) & 0xFFFFFFFFFFFFFFFFULL;
    }
    
    // Inter-round state mixing (in-place, optimized)
    for (int i = 0; i < REGISTER_COUNT; i++) {
        xor_512_inplace(state->registers[i], state->registers[(i + 1) % REGISTER_COUNT]);
        xor_512_inplace(state->registers[i], state->registers[(i + 2) % REGISTER_COUNT]);
    }
}

// Generate keystream (optimized - avoid memcpy overhead)
static void generate_keystream(
    const CipherState* state,
    uint32_t block_number,
    uint8_t* keystream
) {
    // Pre-calculate total size and use stack array (faster than heap)
    const size_t total_size = ACCUMULATOR_SIZE + REGISTER_COUNT * REGISTER_SIZE + 6 + 8;
    uint8_t combined[ACCUMULATOR_SIZE + REGISTER_COUNT * REGISTER_SIZE + 6 + 8];
    
    // Copy accumulator
    memcpy(combined, state->accumulator, ACCUMULATOR_SIZE);
    size_t offset = ACCUMULATOR_SIZE;
    
    // Copy registers (unrolled for better performance)
    memcpy(combined + offset, state->registers[0], REGISTER_SIZE); offset += REGISTER_SIZE;
    memcpy(combined + offset, state->registers[1], REGISTER_SIZE); offset += REGISTER_SIZE;
    memcpy(combined + offset, state->registers[2], REGISTER_SIZE); offset += REGISTER_SIZE;
    memcpy(combined + offset, state->registers[3], REGISTER_SIZE); offset += REGISTER_SIZE;
    memcpy(combined + offset, state->registers[4], REGISTER_SIZE); offset += REGISTER_SIZE;
    memcpy(combined + offset, state->registers[5], REGISTER_SIZE); offset += REGISTER_SIZE;
    memcpy(combined + offset, state->registers[6], REGISTER_SIZE); offset += REGISTER_SIZE;
    
    // Copy domain string
    combined[offset++] = 'R'; combined[offset++] = 'U'; combined[offset++] = 'C';
    combined[offset++] = '-'; combined[offset++] = 'K'; combined[offset++] = 'S';
    
    // Copy block number (little-endian)
    combined[offset++] = block_number & 0xFF;
    combined[offset++] = (block_number >> 8) & 0xFF;
    combined[offset++] = (block_number >> 16) & 0xFF;
    combined[offset++] = (block_number >> 24) & 0xFF;
    combined[offset++] = 0; combined[offset++] = 0; combined[offset++] = 0; combined[offset++] = 0;
    
    shake256_hash(combined, total_size, keystream, BLOCK_SIZE);
}

// Apply ciphertext feedback
static void apply_ciphertext_feedback(CipherState* state, const uint8_t* ciphertext) {
    for (int i = 0; i < REGISTER_COUNT; i++) {
        int feedback_shift = (i * 37) % 256;
        // Simplified feedback (full implementation would use proper bit shifts)
        for (int j = 0; j < REGISTER_SIZE; j++) {
            state->registers[i][j] ^= ciphertext[j % BLOCK_SIZE];
        }
    }
}

// Expand key into key material
void* ruc_expand_key(const uint8_t* key) {
    KeyMaterial* km = (KeyMaterial*)malloc(sizeof(KeyMaterial));
    
    // Generate 7 state registers
    for (int i = 0; i < REGISTER_COUNT; i++) {
        shake256_with_domain(key, KEY_SIZE, "RUC-REG", i, km->registers[i], REGISTER_SIZE);
    }
    
    // Determine selector count
    size_t num_selectors = MIN_SELECTORS + (key[1] % (MAX_SELECTORS - MIN_SELECTORS + 1));
    km->num_selectors = num_selectors;
    
    // Generate selectors (must be odd)
    // Match TypeScript: selector = (selBytes[0] << 8) | selBytes[1] (big-endian)
    for (size_t i = 0; i < num_selectors; i++) {
        uint8_t sel_bytes[2];
        shake256_with_domain(key, KEY_SIZE, "RUC-SEL", i, sel_bytes, 2);
        uint16_t selector = ((uint16_t)sel_bytes[0] << 8) | sel_bytes[1];
        
        if (selector % 2 == 0) selector += 1;
        if (selector == 0) selector = 1;
        
        km->selectors[i] = selector;
    }
    
    // Permute selectors using ChaCha20
    uint8_t permute_seed[32];
    uint8_t permute_input[KEY_SIZE + 8];
    memcpy(permute_input, key, KEY_SIZE);
    memcpy(permute_input + KEY_SIZE, "RUC-PERM", 8);
    shake256_hash(permute_input, KEY_SIZE + 8, permute_seed, 32);
    
    ChaCha20PRNG prng(permute_seed);
    for (int i = num_selectors - 1; i > 0; i--) {
        uint32_t j = prng.next_int(i + 1);
        uint16_t temp = km->selectors[i];
        km->selectors[i] = km->selectors[j];
        km->selectors[j] = temp;
    }
    
    // Generate 24 round keys
    for (int r = 0; r < ROUNDS; r++) {
        shake256_with_domain(key, KEY_SIZE, "RUC-RK", r, km->round_keys[r], REGISTER_SIZE);
    }
    
    // Generate 24 S-boxes
    for (int r = 0; r < ROUNDS; r++) {
        generate_sbox(key, KEY_SIZE, r, km->sboxes[r]);
    }
    
    // Pre-compute key constants for all selectors (major optimization!)
    // This avoids calling SHAKE256 thousands of times per block
    // Only compute for selectors that are actually used (typically 16-31 selectors)
    for (size_t i = 0; i < num_selectors; i++) {
        uint8_t const_seed_input[KEY_SIZE + 8 + 2];
        memcpy(const_seed_input, key, KEY_SIZE);
        memcpy(const_seed_input + KEY_SIZE, "RUC-CONST", 9);
        const_seed_input[KEY_SIZE + 8] = (km->selectors[i] >> 8) & 0xFF;
        const_seed_input[KEY_SIZE + 9] = km->selectors[i] & 0xFF;
        
        uint8_t const_seed[1];
        shake256_hash(const_seed_input, KEY_SIZE + 10, const_seed, 1);
        km->key_constants[i] = const_seed[0];
    }
    
    return km;
}

void ruc_free_key_material(void* km) {
    free(km);
}

// Encrypt a single block
void ruc_encrypt_block(
    const uint8_t* plaintext,
    const uint8_t* key,
    const uint8_t* iv,
    uint32_t block_number,
    void* key_material,
    uint8_t* ciphertext
) {
    KeyMaterial* km = (KeyMaterial*)key_material;
    
    // Create state from key material
    CipherState state;
    memcpy(state.registers, km->registers, REGISTER_COUNT * REGISTER_SIZE);
    memset(state.accumulator, 0, ACCUMULATOR_SIZE);
    
    // Mix IV into state (simplified - full implementation would use proper rotations)
    uint8_t iv_expanded[REGISTER_SIZE];
    uint8_t iv_input[IV_SIZE + 14]; // "RUC-IV-EXPAND" is 13 bytes + null terminator
    memcpy(iv_input, iv, IV_SIZE);
    memcpy(iv_input + IV_SIZE, "RUC-IV-EXPAND", 13);
    shake256_hash(iv_input, IV_SIZE + 13, iv_expanded, REGISTER_SIZE);
    
    for (int i = 0; i < REGISTER_COUNT; i++) {
        xor_512_inplace(state.registers[i], iv_expanded);
    }
    
    // Incorporate counter (CTR mode)
    uint8_t counter_bytes[8];
    for (int i = 0; i < 8; i++) {
        counter_bytes[i] = (block_number >> (i * 8)) & 0xFF;
    }
    uint8_t counter_hash[REGISTER_SIZE];
    uint8_t ctr_input[8 + 3];
    memcpy(ctr_input, counter_bytes, 8);
    memcpy(ctr_input + 8, "CTR", 3);
    shake256_hash(ctr_input, 11, counter_hash, REGISTER_SIZE);
    xor_512(state.registers[0], counter_hash, state.registers[0]);
    
    // Order selectors
    uint16_t ordered_selectors[MAX_SELECTORS];
    size_t selector_indices[MAX_SELECTORS];
    order_selectors(km, key, iv, block_number, ordered_selectors, selector_indices);
    
    // Execute all rounds
    for (int r = 0; r < ROUNDS; r++) {
        execute_round(&state, r, ordered_selectors, selector_indices, km->num_selectors, km, key);
    }
    
    // Generate keystream
    uint8_t keystream[BLOCK_SIZE];
    generate_keystream(&state, block_number, keystream);
    
    // XOR plaintext with keystream
    for (int i = 0; i < BLOCK_SIZE; i++) {
        ciphertext[i] = plaintext[i] ^ keystream[i];
    }
    
    // Apply ciphertext feedback
    apply_ciphertext_feedback(&state, ciphertext);
}

// Decrypt a single block (same as encrypt for XOR cipher)
void ruc_decrypt_block(
    const uint8_t* ciphertext,
    const uint8_t* key,
    const uint8_t* iv,
    uint32_t block_number,
    void* key_material,
    uint8_t* plaintext
) {
    ruc_encrypt_block(ciphertext, key, iv, block_number, key_material, plaintext);
}

// Encrypt multiple blocks in batch (optimized with caching)
void ruc_encrypt_blocks_batch(
    const uint8_t* plaintext_blocks,
    size_t num_blocks,
    const uint8_t* key,
    const uint8_t* iv,
    uint32_t start_block_number,
    void* key_material,
    uint8_t* ciphertext_blocks
) {
    KeyMaterial* km = (KeyMaterial*)key_material;
    
    // Reset profiling counters for this batch
    profile_shake256_calls = 0;
    profile_rounds_executed = 0;
    profile_selector_ordering_calls = 0;
    profile_keystream_calls = 0;
    profile_counter_hash_calls = 0;
    profile_gf_mul_calls = 0;
    profile_register_ops_calls = 0;
    profile_blocks_processed = num_blocks; // Set total blocks for this batch
    
    // Pre-compute IV expansion once (same for all blocks with same IV) - MAJOR OPTIMIZATION!
    uint8_t iv_expanded[REGISTER_SIZE];
    uint8_t iv_input[IV_SIZE + 14];
    memcpy(iv_input, iv, IV_SIZE);
    memcpy(iv_input + IV_SIZE, "RUC-IV-EXPAND", 13);
    shake256_hash(iv_input, IV_SIZE + 13, iv_expanded, REGISTER_SIZE);
    
    // Process all blocks
    for (size_t i = 0; i < num_blocks; i++) {
        uint32_t block_number = start_block_number + i;
        
        // Create state from key material
        CipherState state;
        memcpy(state.registers, km->registers, REGISTER_COUNT * REGISTER_SIZE);
        memset(state.accumulator, 0, ACCUMULATOR_SIZE);
        
        // Mix pre-computed IV into state (no SHAKE256 call per block!) - in-place
        for (int j = 0; j < REGISTER_COUNT; j++) {
            xor_512_inplace(state.registers[j], iv_expanded);
        }
        
        // Incorporate counter (CTR mode)
        uint8_t counter_bytes[8];
        for (int j = 0; j < 8; j++) {
            counter_bytes[j] = (block_number >> (j * 8)) & 0xFF;
        }
        uint8_t counter_hash[REGISTER_SIZE];
        uint8_t ctr_input[8 + 3];
        memcpy(ctr_input, counter_bytes, 8);
        memcpy(ctr_input + 8, "CTR", 3);
        profile_counter_hash_calls++;
        shake256_hash(ctr_input, 11, counter_hash, REGISTER_SIZE);
        xor_512_inplace(state.registers[0], counter_hash);
        
        // Order selectors (uses SHAKE256 but necessary for security)
        profile_selector_ordering_calls++;
        uint16_t ordered_selectors[MAX_SELECTORS];
        size_t selector_indices[MAX_SELECTORS];
        order_selectors(km, key, iv, block_number, ordered_selectors, selector_indices);
        
        // Execute all rounds
        profile_rounds_executed += ROUNDS;
        for (int r = 0; r < ROUNDS; r++) {
            execute_round(&state, r, ordered_selectors, selector_indices, km->num_selectors, km, key);
        }
        
        // Generate keystream
        profile_keystream_calls++;
        uint8_t keystream[BLOCK_SIZE];
        generate_keystream(&state, block_number, keystream);
        
        // XOR plaintext with keystream
        const uint8_t* plaintext = plaintext_blocks + i * BLOCK_SIZE;
        uint8_t* ciphertext = ciphertext_blocks + i * BLOCK_SIZE;
        for (int j = 0; j < BLOCK_SIZE; j++) {
            ciphertext[j] = plaintext[j] ^ keystream[j];
        }
        
        // Apply ciphertext feedback
        apply_ciphertext_feedback(&state, ciphertext);
    }
}

// Get profiling statistics
void ruc_get_profile_stats(
    uint64_t* shake256_calls,
    uint64_t* shake256_time_us,
    uint64_t* rounds_time_us,
    uint64_t* selector_ordering_time_us,
    uint64_t* keystream_time_us,
    uint64_t* counter_hash_time_us,
    uint64_t* gf_mul_calls,
    uint64_t* register_ops_calls
) {
    *shake256_calls = profile_shake256_calls;
    *shake256_time_us = 0; // Would need timing, but counting is enough for now
    *rounds_time_us = profile_rounds_executed;
    *selector_ordering_time_us = profile_selector_ordering_calls;
    *keystream_time_us = profile_keystream_calls;
    *counter_hash_time_us = profile_counter_hash_calls;
    *gf_mul_calls = profile_gf_mul_calls;
    *register_ops_calls = profile_register_ops_calls;
}

// Decrypt multiple blocks in batch
void ruc_decrypt_blocks_batch(
    const uint8_t* ciphertext_blocks,
    size_t num_blocks,
    const uint8_t* key,
    const uint8_t* iv,
    uint32_t start_block_number,
    void* key_material,
    uint8_t* plaintext_blocks
) {
    ruc_encrypt_blocks_batch(
        ciphertext_blocks,
        num_blocks,
        key,
        iv,
        start_block_number,
        key_material,
        plaintext_blocks
    );
}

