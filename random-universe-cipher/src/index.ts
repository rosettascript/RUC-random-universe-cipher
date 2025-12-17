/**
 * Random Universe Cipher - Demo Entry Point
 */

import { initDemo } from './ui/demo';
import { preloadCppWASM } from './cipher/wasm-preload';

// Preload C++ WASM in background for faster first use
preloadCppWASM().catch(() => {
  // Silently fail - will retry on first use
});

// Initialize when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initDemo);
} else {
  initDemo();
}

// Export cipher for console access
export * from './cipher';

