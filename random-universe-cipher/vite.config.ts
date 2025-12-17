import { defineConfig } from 'vite';
import wasm from 'vite-plugin-wasm';
import topLevelAwait from 'vite-plugin-top-level-await';

// GitHub Pages base path - set via environment variable or default to '/'
// For GitHub Pages: set BASE_PATH to your repository name (e.g., '/random-universe/')
const base = process.env.BASE_PATH || '/';

export default defineConfig({
  base,
  root: '.',
  plugins: [wasm(), topLevelAwait()],
  build: {
    outDir: 'dist',
    sourcemap: true,
    target: 'esnext',
    // Copy WASM files to dist
    copyPublicDir: false,
    rollupOptions: {
      output: {
        // Ensure WASM files are included
        assetFileNames: (assetInfo) => {
          if (assetInfo.name?.endsWith('.wasm')) {
            return 'assets/[name][extname]';
          }
          return 'assets/[name]-[hash][extname]';
        },
      },
    },
  },
  server: {
    port: 3000,
    open: true,
  },
  worker: {
    format: 'es',
  },
  test: {
    globals: true,
    environment: 'node',
  },
});

