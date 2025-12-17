#ifndef CHACHA20_H
#define CHACHA20_H

#include <cstdint>
#include <cstddef>

// ChaCha20 PRNG class
class ChaCha20PRNG {
private:
    uint8_t key[32];
    uint8_t nonce[12];
    uint32_t counter;
    uint8_t buffer[64];
    size_t buffer_pos;
    
    void generate_block();
    
public:
    ChaCha20PRNG(const uint8_t* seed_key, const uint8_t* seed_nonce = nullptr);
    void next_bytes(uint8_t* output, size_t count);
    uint32_t next_u32();
    uint32_t next_int(uint32_t max);
    void reset();
};

#endif // CHACHA20_H

