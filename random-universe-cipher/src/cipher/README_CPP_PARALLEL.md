# C++ WASM Parallel Processing

This module provides ultra-fast encryption/decryption using C++ WebAssembly with automatic parallel processing based on CPU core count.

## Quick Start

```typescript
import { encryptCppParallel, decryptCppParallel, getCpuCoreCount } from './cipher';

// Automatically uses all CPU cores
const encrypted = await encryptCppParallel(plaintext, key);
const decrypted = await decryptCppParallel(encrypted, key);

console.log(`Using ${getCpuCoreCount()} CPU cores`);
```

## Features

- ✅ **Pure C++ WASM** - Maximum performance
- ✅ **Automatic Parallelization** - Uses all CPU cores
- ✅ **100% Compatible** - Same encryption logic as TypeScript
- ✅ **Progress Callbacks** - Track encryption progress
- ✅ **Error Handling** - Graceful fallback to JavaScript

## Performance

Expected speedups:
- **5-10x** faster than pure JavaScript (C++ WASM)
- **2-4x** additional speedup with parallel processing
- **Combined: 10-40x** faster for large files

## API

### `encryptCppParallel(plaintext, key, nonce?, onProgress?)`

Encrypt data using C++ WASM with parallel processing.

**Parameters:**
- `plaintext: Uint8Array` - Data to encrypt
- `key: Uint8Array` - 64-byte encryption key
- `nonce?: Uint8Array` - Optional 16-byte nonce (auto-generated if not provided)
- `onProgress?: (progress: number) => void` - Optional progress callback (0-100)

**Returns:** `Promise<Uint8Array>` - Encrypted data (nonce prepended)

### `decryptCppParallel(ciphertext, key, onProgress?)`

Decrypt data using C++ WASM with parallel processing.

**Parameters:**
- `ciphertext: Uint8Array` - Data to decrypt (nonce prepended)
- `key: Uint8Array` - 64-byte decryption key
- `onProgress?: (progress: number) => void` - Optional progress callback (0-100)

**Returns:** `Promise<Uint8Array>` - Decrypted data

### `getCpuCoreCount(): number`

Get the number of CPU cores detected.

**Returns:** `number` - Number of CPU cores (defaults to 4 if unavailable)

## Example

```typescript
import { encryptCppParallel, decryptCppParallel } from './cipher';
import { generateRandomKey } from './cipher';

// Generate a key
const key = generateRandomKey();

// Encrypt with progress tracking
const plaintext = new TextEncoder().encode('Hello, World!');
const encrypted = await encryptCppParallel(
  plaintext,
  key,
  undefined,
  (progress) => {
    console.log(`Encryption: ${progress}%`);
  }
);

// Decrypt
const decrypted = await decryptCppParallel(
  encrypted,
  key,
  (progress) => {
    console.log(`Decryption: ${progress}%`);
  }
);

console.log(new TextDecoder().decode(decrypted)); // "Hello, World!"
```

## Building

Before using, you need to build the C++ WASM module:

```bash
# Install Emscripten SDK first (see CPP_WASM_OPTIMIZATION.md)
cd cpp-wasm
./build.sh
```

## Troubleshooting

### WASM Module Not Found

If you see errors about the WASM module not loading:
1. Make sure you've built the C++ WASM module (`cd cpp-wasm && ./build.sh`)
2. Check that `cpp-wasm/pkg/ruc_wasm.wasm` exists
3. Verify Vite configuration includes WASM plugin

### Workers Not Starting

If parallel processing isn't working:
1. Check browser console for errors
2. Verify browser supports Web Workers
3. Check that `navigator.hardwareConcurrency` is available

### Performance Issues

If performance isn't as expected:
1. Verify C++ WASM module is loading (check console logs)
2. Check number of CPU cores detected
3. Ensure you're processing large enough files to benefit from parallelization

## Fallback Behavior

If C++ WASM is unavailable, the functions will throw an error. For automatic fallback to JavaScript, use:

```typescript
import { encryptFast, decryptFast } from './cipher';
```

These functions will automatically fall back to JavaScript if WASM is unavailable.

