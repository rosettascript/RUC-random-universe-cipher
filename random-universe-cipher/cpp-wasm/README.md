# C++ WASM Implementation

This directory contains the C++ WebAssembly implementation of the Random Universe Cipher.

## Building

### Prerequisites

1. Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html
2. Ensure `emcc` is in your PATH

### Build Steps

```bash
cd cpp-wasm
chmod +x build.sh
./build.sh
```

This will generate:
- `pkg/ruc_wasm.js` - JavaScript wrapper
- `pkg/ruc_wasm.wasm` - WebAssembly binary

## Architecture

The C++ implementation maintains 100% compatibility with the TypeScript encryption logic:

- **GF(2^8) Math**: Galois field arithmetic with polynomial 0x1B
- **SHAKE256**: Keccak-based hash function
- **ChaCha20**: PRNG for selector ordering
- **S-box Generation**: Key-derived substitution boxes
- **24-Round Cipher**: Full encryption/decryption rounds

## Parallel Processing

The TypeScript integration layer (`modes-cpp-parallel.ts`) automatically:
- Detects CPU core count using `navigator.hardwareConcurrency`
- Distributes blocks across workers for parallel processing
- Uses C++ WASM for maximum performance

## Performance

Expected performance improvements:
- **5-10x faster** than pure JavaScript
- **2-4x faster** with parallel processing on multi-core systems
- **Combined**: Up to 20-40x faster for large files

## Integration

The C++ WASM module is integrated via:
- `src/cipher/parallel-worker.ts` - Worker pool management
- `src/cipher/modes-cpp-parallel.ts` - Encryption/decryption functions

Use `encryptCppParallel()` and `decryptCppParallel()` for maximum performance.

