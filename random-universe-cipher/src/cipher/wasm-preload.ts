/**
 * WASM Preloader
 * 
 * Preloads C++ WASM module at startup for faster first use
 */

import { ParallelWorkerPool } from './parallel-worker';

let preloadPromise: Promise<boolean> | null = null;
let preloadComplete = false;

/**
 * Preload C++ WASM module (call at startup)
 */
export async function preloadCppWASM(): Promise<boolean> {
  if (preloadComplete) {
    return true;
  }
  
  if (preloadPromise) {
    return preloadPromise;
  }
  
  preloadPromise = (async () => {
    try {
      const pool = new ParallelWorkerPool();
      const loaded = await pool.initWASM();
      if (loaded) {
        preloadComplete = true;
        console.log('✅ C++ WASM preloaded successfully');
      }
      return loaded;
    } catch (error) {
      console.warn('⚠️ C++ WASM preload failed (will try again on first use):', error);
      return false;
    }
  })();
  
  return preloadPromise;
}

/**
 * Check if WASM is preloaded
 */
export function isCppWASMPreloaded(): boolean {
  return preloadComplete;
}

