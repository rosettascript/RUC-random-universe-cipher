/**
 * Random Universe Cipher - C++ WASM Parallel Processing Modes
 * 
 * Ultra-fast encryption/decryption using C++ WASM with parallel processing
 * based on CPU core count
 */

import { BYTES, DOMAIN } from './constants';
import { shake256Hash } from './shake256';
import { concatBytes, stringToBytes, randomBytes } from './bigint-utils';
import { ParallelWorkerPool, getCpuCoreCount } from './parallel-worker';
import { encryptCTRFast, decryptCTRFast } from './modes-fast';

// Global worker pool
let workerPool: ParallelWorkerPool | null = null;
let wasmAvailable = false;
let wasmInitialized = false;

/**
 * Initialize the parallel worker pool with fallback
 */
async function initWorkerPool(): Promise<ParallelWorkerPool | null> {
  if (wasmInitialized) {
    return wasmAvailable ? workerPool : null;
  }
  
  wasmInitialized = true;
  
  try {
    if (!workerPool) {
      workerPool = new ParallelWorkerPool();
    }
    wasmAvailable = await workerPool.initWASM();
    if (!wasmAvailable) {
      // Don't log warning here - let the caller handle it
      return null;
    }
    return workerPool;
  } catch (error) {
    // Don't log warning here - let the caller handle it
    wasmAvailable = false;
    return null;
  }
}

/**
 * PKCS#7 padding
 */
function pkcs7Pad(data: Uint8Array, blockSize: number): Uint8Array {
  const paddingLen = blockSize - (data.length % blockSize);
  const padded = new Uint8Array(data.length + paddingLen);
  padded.set(data);
  for (let i = data.length; i < padded.length; i++) {
    padded[i] = paddingLen;
  }
  return padded;
}

/**
 * Remove PKCS#7 padding
 */
function pkcs7Unpad(data: Uint8Array): Uint8Array {
  if (data.length === 0) {
    throw new Error('Cannot unpad empty data');
  }
  
  const paddingLen = data[data.length - 1];
  
  if (paddingLen === 0 || paddingLen > data.length || paddingLen > BYTES.BLOCK) {
    throw new Error('Invalid padding');
  }
  
  // Verify all padding bytes
  for (let i = data.length - paddingLen; i < data.length; i++) {
    if (data[i] !== paddingLen) {
      throw new Error('Invalid padding');
    }
  }
  
  return data.subarray(0, data.length - paddingLen);
}

/**
 * Ultra-fast CTR Mode Encryption with C++ WASM and parallel processing
 * 
 * Uses C++ WASM for maximum performance and distributes blocks across
 * multiple CPU cores for parallel processing.
 * 
 * @param plaintext - Data to encrypt
 * @param key - Encryption key
 * @param nonce - Optional 16-byte nonce (auto-generated if not provided)
 * @param onProgress - Optional progress callback (0-100)
 */
export async function encryptCTRCppParallel(
  plaintext: Uint8Array,
  key: Uint8Array,
  nonce?: Uint8Array,
  onProgress?: (progress: number) => void
): Promise<Uint8Array> {
  // Try to initialize C++ WASM, fallback to JavaScript if unavailable
  const pool = await initWorkerPool();
  
  if (!pool) {
    // Fallback - throw error so modes-fast.ts can handle it
    throw new Error('C++ WASM parallel processing not available');
  }
  
  const startTime = performance.now();
  
  // Generate nonce if not provided
  const actualNonce = nonce || randomBytes(BYTES.NONCE);
  if (actualNonce.length !== BYTES.NONCE) {
    throw new Error(`Nonce must be ${BYTES.NONCE} bytes`);
  }
  
  // Derive IV from nonce
  const iv = shake256Hash(
    concatBytes(actualNonce, stringToBytes(DOMAIN.CTR_IV)),
    BYTES.IV
  );
  
  // Pad plaintext
  const padded = pkcs7Pad(plaintext, BYTES.BLOCK);
  const numBlocks = padded.length / BYTES.BLOCK;
  
  // Allocate output: nonce + ciphertext
  const output = new Uint8Array(BYTES.NONCE + padded.length);
  output.set(actualNonce, 0);
  
  if (onProgress) onProgress(5);
  
  // Process blocks in parallel using C++ WASM
  const ciphertext = await pool.processBlocksParallel(
    padded,
    numBlocks,
    key,
    iv,
    0,
    true,
    (progress) => {
      if (onProgress) {
        // Map worker progress (0-100) to overall progress (5-95)
        onProgress(5 + Math.floor(progress * 0.9));
      }
    }
  );
  
  // Copy ciphertext to output
  output.set(ciphertext, BYTES.NONCE);
  
  if (onProgress) onProgress(100);
  
  const endTime = performance.now();
  const totalTime = ((endTime - startTime) / 1000).toFixed(2);
  const mbSize = (plaintext.length / (1024 * 1024)).toFixed(2);
  const mbPerSec = (plaintext.length / (1024 * 1024) / ((endTime - startTime) / 1000)).toFixed(2);
  const cores = getCpuCoreCount();
  console.log(`⚡ C++ WASM (${cores} cores): Encrypted ${mbSize}MB in ${totalTime}s (${mbPerSec} MB/s)`);
  
  return output;
}

/**
 * Ultra-fast CTR Mode Decryption with C++ WASM and parallel processing
 * 
 * @param ciphertext - Data to decrypt
 * @param key - Decryption key
 * @param onProgress - Optional progress callback (0-100)
 */
export async function decryptCTRCppParallel(
  ciphertext: Uint8Array,
  key: Uint8Array,
  onProgress?: (progress: number) => void
): Promise<Uint8Array> {
  // Try to initialize C++ WASM, fallback to JavaScript if unavailable
  const pool = await initWorkerPool();
  
  if (!pool) {
    // Fallback - throw error so modes-fast.ts can handle it
    throw new Error('C++ WASM parallel processing not available');
  }
  
  const startTime = performance.now();
  
  if (ciphertext.length < BYTES.NONCE + BYTES.BLOCK) {
    throw new Error('Ciphertext too short');
  }
  
  // Extract nonce
  const nonce = ciphertext.subarray(0, BYTES.NONCE);
  const encryptedData = ciphertext.subarray(BYTES.NONCE);
  
  if (encryptedData.length % BYTES.BLOCK !== 0) {
    throw new Error('Invalid ciphertext length');
  }
  
  // Derive IV from nonce
  const iv = shake256Hash(
    concatBytes(nonce, stringToBytes(DOMAIN.CTR_IV)),
    BYTES.IV
  );
  
  const numBlocks = encryptedData.length / BYTES.BLOCK;
  
  if (onProgress) onProgress(5);
  
  // Process blocks in parallel using C++ WASM
  const padded = await pool.processBlocksParallel(
    encryptedData,
    numBlocks,
    key,
    iv,
    0,
    false,
    (progress) => {
      if (onProgress) {
        // Map worker progress (0-100) to overall progress (5-95)
        onProgress(5 + Math.floor(progress * 0.9));
      }
    }
  );
  
  if (onProgress) onProgress(100);
  
  // Remove padding
  const decrypted = pkcs7Unpad(padded);
  
  const endTime = performance.now();
  const totalTime = ((endTime - startTime) / 1000).toFixed(2);
  const mbSize = (decrypted.length / (1024 * 1024)).toFixed(2);
  const mbPerSec = (decrypted.length / (1024 * 1024) / ((endTime - startTime) / 1000)).toFixed(2);
  const cores = getCpuCoreCount();
  console.log(`⚡ C++ WASM (${cores} cores): Decrypted ${mbSize}MB in ${totalTime}s (${mbPerSec} MB/s)`);
  
  return decrypted;
}

/**
 * Ultra-fast encryption with auto-generated nonce
 */
export async function encryptCppParallel(
  plaintext: Uint8Array,
  key: Uint8Array,
  onProgress?: (progress: number) => void
): Promise<Uint8Array> {
  return encryptCTRCppParallel(plaintext, key, undefined, onProgress);
}

/**
 * Ultra-fast decryption
 */
export async function decryptCppParallel(
  ciphertext: Uint8Array,
  key: Uint8Array,
  onProgress?: (progress: number) => void
): Promise<Uint8Array> {
  return decryptCTRCppParallel(ciphertext, key, onProgress);
}

