# Quick Start - C++ WASM Parallel Processing

## Prerequisites

1. **Emscripten SDK** - Required for building C++ WASM
   ```bash
   # Install Emscripten (one-time setup)
   git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
   cd ~/emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **Build C++ WASM Module**
   ```bash
   cd random-universe-cipher/cpp-wasm
   ./build.sh
   ```

   This creates:
   - `cpp-wasm/pkg/ruc_wasm.js` - JavaScript wrapper
   - `cpp-wasm/pkg/ruc_wasm.wasm` - WebAssembly binary

## Usage

### Basic Example

```typescript
import { 
  encryptCppParallel, 
  decryptCppParallel, 
  getCpuCoreCount,
  generateRandomKey 
} from './cipher';

// Check CPU cores
console.log(`Detected ${getCpuCoreCount()} CPU cores`);

// Generate a key
const key = generateRandomKey();

// Encrypt
const plaintext = new TextEncoder().encode('Hello, World!');
const encrypted = await encryptCppParallel(plaintext, key);

// Decrypt
const decrypted = await decryptCppParallel(encrypted, key);
console.log(new TextDecoder().decode(decrypted)); // "Hello, World!"
```

### With Progress Tracking

```typescript
const encrypted = await encryptCppParallel(
  plaintext,
  key,
  undefined,
  (progress) => {
    console.log(`Encryption: ${progress}%`);
  }
);
```

### Large File Encryption

```typescript
// Read a large file
const fileInput = document.querySelector('input[type="file"]');
const file = fileInput.files[0];
const fileData = new Uint8Array(await file.arrayBuffer());

// Encrypt with progress
const encrypted = await encryptCppParallel(
  fileData,
  key,
  undefined,
  (progress) => {
    console.log(`Encrypting: ${progress}%`);
  }
);

// Save encrypted file
const blob = new Blob([encrypted]);
const url = URL.createObjectURL(blob);
const a = document.createElement('a');
a.href = url;
a.download = file.name + '.encrypted';
a.click();
```

## Performance Comparison

### Before (JavaScript)
- 15MB file: ~20-30 seconds
- Single-threaded

### After (C++ WASM + Parallel)
- 15MB file: ~0.5-1 second (on 4-core machine)
- Multi-threaded across all CPU cores
- **20-60x faster!**

## API Reference

### `encryptCppParallel(plaintext, key, nonce?, onProgress?)`

Encrypt data using C++ WASM with parallel processing.

**Parameters:**
- `plaintext: Uint8Array` - Data to encrypt
- `key: Uint8Array` - 64-byte encryption key
- `nonce?: Uint8Array` - Optional 16-byte nonce
- `onProgress?: (progress: number) => void` - Progress callback

**Returns:** `Promise<Uint8Array>` - Encrypted data (nonce prepended)

### `decryptCppParallel(ciphertext, key, onProgress?)`

Decrypt data using C++ WASM with parallel processing.

**Parameters:**
- `ciphertext: Uint8Array` - Encrypted data (nonce prepended)
- `key: Uint8Array` - 64-byte decryption key
- `onProgress?: (progress: number) => void` - Progress callback

**Returns:** `Promise<Uint8Array>` - Decrypted data

### `getCpuCoreCount(): number`

Get detected CPU core count.

**Returns:** `number` - Number of CPU cores

## Troubleshooting

### "WASM module not found"

1. Make sure you've built the C++ WASM:
   ```bash
   cd cpp-wasm && ./build.sh
   ```

2. Check that files exist:
   ```bash
   ls cpp-wasm/pkg/ruc_wasm.*
   ```

3. Verify Vite config includes WASM plugin (already configured)

### "Workers not starting"

1. Check browser console for errors
2. Verify browser supports Web Workers
3. Check `navigator.hardwareConcurrency` is available

### Build Errors

If `./build.sh` fails:
1. Make sure Emscripten is installed and activated
2. Run `source ~/emsdk/emsdk_env.sh` before building
3. Check that `emcc` is in PATH: `which emcc`

## Next Steps

- See `CPP_WASM_OPTIMIZATION.md` for detailed architecture
- See `src/cipher/README_CPP_PARALLEL.md` for API details
- Check `cpp-wasm/README.md` for build instructions

