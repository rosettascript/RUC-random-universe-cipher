#ifndef GF_MATH_H
#define GF_MATH_H

#include <cstdint>

// GF(2^8) multiplication with polynomial 0x1B
uint8_t gf_mul(uint8_t a, uint8_t b);

// Multiply each byte of a 64-byte register by a constant
void gf_mul_register(const uint8_t* reg, uint8_t multiplier, uint8_t* result);

// Multiply each byte of a 64-byte register by a constant (in-place)
void gf_mul_register_inplace(uint8_t* reg, uint8_t multiplier);

#endif // GF_MATH_H

