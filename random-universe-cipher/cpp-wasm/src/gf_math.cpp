#include "gf_math.h"
#include <cstddef>

// GF(2^8) multiplication using log/exp tables (much smaller than full lookup table)
// Only 512 bytes total (256 log + 256 exp) vs 65KB for full table
static uint8_t gf_log_table[256];
static uint8_t gf_exp_table[256];
static bool gf_tables_initialized = false;

// Initialize log/exp tables (called once)
static void init_gf_tables() {
    if (gf_tables_initialized) return;
    
    // Find a primitive element (generator) for GF(2^8)
    // Using 3 as generator (standard for AES polynomial 0x1B)
    uint8_t generator = 3;
    
    // Build log table: log_table[value] = exponent such that generator^exponent = value
    uint8_t val = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp_table[i] = val;
        gf_log_table[val] = i;
        val = (val << 1) ^ ((val & 0x80) ? 0x1B : 0); // Multiply by generator
    }
    gf_exp_table[255] = 1; // Wrap around
    gf_log_table[0] = 0; // Special case: log(0) = 0 (but 0 * anything = 0)
    gf_log_table[1] = 0;
    
    gf_tables_initialized = true;
}

// GF(2^8) multiplication using log/exp tables (O(1) with small cache footprint)
// Optimized: Initialize tables at module load time (static initialization)
__attribute__((constructor))
static void init_gf_tables_auto() {
    init_gf_tables();
}

uint8_t gf_mul(uint8_t a, uint8_t b) {
    // Handle zero case (fast path)
    if (a == 0 || b == 0) return 0;
    
    // a * b = exp(log(a) + log(b)) mod 255
    // Optimized: Branchless mod 255 using bitwise operations
    uint16_t sum = gf_log_table[a] + gf_log_table[b];
    // Branchless modulo 255 (two passes for sum < 510)
    sum = (sum & 0xFF) + (sum >> 8); // sum % 255 for sum < 510
    sum = (sum & 0xFF) + (sum >> 8); // second pass for sum < 510
    
    return gf_exp_table[sum];
}

// Multiply each byte of a 64-byte register by a constant
void gf_mul_register(const uint8_t* reg, uint8_t multiplier, uint8_t* result) {
    for (size_t i = 0; i < 64; i++) {
        result[i] = gf_mul(reg[i], multiplier);
    }
}

// Multiply each byte of a 64-byte register by a constant (in-place)
void gf_mul_register_inplace(uint8_t* reg, uint8_t multiplier) {
    for (size_t i = 0; i < 64; i++) {
        reg[i] = gf_mul(reg[i], multiplier);
    }
}

