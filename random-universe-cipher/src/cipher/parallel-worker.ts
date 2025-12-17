/**
 * Parallel Worker for C++ WASM Encryption
 * 
 * Processes blocks in parallel using multiple Web Workers
 * based on the user's CPU core count
 */

export interface ParallelWorkerMessage {
  type: 'encrypt' | 'decrypt';
  id: string;
  blocks: Uint8Array;
  startBlockNumber: number;
  numBlocks: number;
  key: Uint8Array;
  iv: Uint8Array;
}

export interface ParallelWorkerResponse {
  type: 'success' | 'error' | 'progress';
  id: string;
  data?: Uint8Array;
  error?: string;
  progress?: number;
}

/**
 * Get the number of CPU cores available
 */
export function getCpuCoreCount(): number {
  // Use navigator.hardwareConcurrency if available, otherwise default to 4
  return navigator.hardwareConcurrency || 4;
}

/**
 * Worker pool with persistent workers for better performance
 */
interface WorkerTask {
  id: string;
  resolve: (data: Uint8Array) => void;
  reject: (error: Error) => void;
  message: ParallelWorkerMessage;
}

export class ParallelWorkerPool {
  private workers: Worker[] = [];
  private numWorkers: number;
  private workerUrl: string;
  private wasmModule: any = null;
  private wasmInitialized = false;
  private workerReady: boolean[] = [];
  private taskQueue: WorkerTask[] = [];
  private activeTasks: Map<string, WorkerTask> = new Map();
  private nextWorkerIndex = 0;
  
  constructor(numWorkers?: number) {
    this.numWorkers = numWorkers || getCpuCoreCount();
    // Create a blob URL for the worker script
    const workerCode = `
      importScripts('${new URL('../worker/cpp-wasm-worker.ts', import.meta.url).href}');
    `;
    this.workerUrl = URL.createObjectURL(new Blob([workerCode], { type: 'application/javascript' }));
    console.log(`🚀 Initializing parallel worker pool with ${this.numWorkers} workers`);
  }
  
  /**
   * Initialize WASM module
   */
  async initWASM(): Promise<boolean> {
    if (this.wasmInitialized) {
      return this.wasmModule !== null;
    }
    
    this.wasmInitialized = true;
    
    try {
      // Import C++ WASM module (placeholder exists for Vite, actual loading happens at runtime)
      const wasm = await import('../../cpp-wasm/pkg/ruc_wasm');
      
      // Try to initialize - placeholder will reject, actual WASM will succeed
      try {
        // Initialize WASM module - default() returns the Module object (or Promise if not ready)
        const module = await wasm.default();
        
        // Wait for runtime to be fully initialized if needed
        // Emscripten may return a Promise if runtime isn't ready
        const actualModule = module instanceof Promise ? await module : module;
        
        // After initialization, functions are on the Module object
        // Emscripten exports C functions with underscore prefix
        // Functions are accessible as module._ruc_expand_key or module["_ruc_expand_key"]
        const hasExpandKey = typeof actualModule._ruc_expand_key === 'function' || 
                            typeof actualModule['_ruc_expand_key'] === 'function';
        const hasMalloc = typeof actualModule._malloc === 'function' || 
                         typeof actualModule['_malloc'] === 'function';
        const hasHeap = (actualModule.HEAPU8 && actualModule.HEAPU8.buffer) ||
                       (actualModule['HEAPU8'] && actualModule['HEAPU8'].buffer);
        
        if (hasExpandKey && hasMalloc && hasHeap) {
          this.wasmModule = actualModule;
          console.log('✅ C++ WASM loaded successfully');
          
          // Create persistent workers
          await this.createWorkers();
          
          return true;
        } else {
          // Debug: show what's actually available
          const allKeys = Object.keys(actualModule);
          const rucKeys = allKeys.filter(k => k.includes('ruc') || k.includes('malloc') || k === 'HEAPU8');
          console.warn('⚠️ C++ WASM loaded but functions missing:', {
            hasExpandKey,
            hasMalloc,
            hasHeap,
            availableKeys: rucKeys.slice(0, 20),
            totalKeys: allKeys.length
          });
        }
      } catch (e) {
        // Placeholder rejects, or WASM not properly initialized
        console.warn('⚠️ C++ WASM initialization error:', e);
        this.wasmModule = null;
        return false;
      }
    } catch (error) {
      console.warn('⚠️ C++ WASM import failed:', error);
      this.wasmModule = null;
      return false;
    }
    
    // If we get here, something is wrong
    this.wasmModule = null;
    return false;
  }
  
  /**
   * Create persistent workers
   */
  private async createWorkers(): Promise<void> {
    const readyPromises: Promise<void>[] = [];
    
    // Create all workers upfront
    for (let i = 0; i < this.numWorkers; i++) {
      const worker = new Worker(
        new URL('../worker/cpp-wasm-worker.ts', import.meta.url),
        { type: 'module' }
      );
      
      this.workerReady[i] = false;
      
      let readyResolve: (() => void) | null = null;
      const readyPromise = new Promise<void>((resolve) => {
        readyResolve = resolve;
      });
      readyPromises.push(readyPromise);
      
      // Set up message handler
      worker.onmessage = (event: MessageEvent<ParallelWorkerResponse>) => {
        const response = event.data;
        
        // Handle ready signal
        if (response.id === 'ready' && response.type === 'success') {
          this.workerReady[i] = true;
          if (readyResolve) {
            readyResolve();
            readyResolve = null;
          }
          return;
        }
        
        // Handle regular messages
        this.handleWorkerMessage(i, response);
      };
      
      worker.onerror = (error) => {
        console.error(`Worker ${i} error:`, error);
        this.workerReady[i] = false;
        // Find and reject any pending tasks for this worker
        this.taskQueue = this.taskQueue.filter(task => {
          if (task.message.id.startsWith(`worker-${i}-`)) {
            task.reject(new Error(`Worker ${i} error`));
            return false;
          }
          return true;
        });
        // Resolve ready promise on error (so we don't hang)
        if (readyResolve) {
          readyResolve();
          readyResolve = null;
        }
      };
      
      this.workers[i] = worker;
    }
    
    // Wait for all workers to be ready (with timeout)
    await Promise.race([
      Promise.all(readyPromises),
      new Promise<void>((resolve) => {
        setTimeout(() => {
          console.warn('Worker ready timeout, assuming workers are ready');
          // Mark all as ready
          for (let i = 0; i < this.numWorkers; i++) {
            this.workerReady[i] = true;
          }
          resolve();
        }, 5000); // 5 second timeout
      })
    ]);
    
    console.log(`✅ All ${this.numWorkers} workers ready`);
  }
  
  /**
   * Handle messages from workers
   */
  private handleWorkerMessage(workerIndex: number, response: ParallelWorkerResponse): void {
    // Ignore ready signals (handled in createWorkers)
    if (response.id === 'ready') {
      return;
    }
    
    // Find the task in active tasks map
    const task = this.activeTasks.get(response.id);
    
    if (!task) {
      console.warn(`No task found for response id: ${response.id}`);
      return;
    }
    
    // Remove from active tasks
    this.activeTasks.delete(response.id);
    this.workerReady[workerIndex] = true;
    
    if (response.type === 'success' && response.data) {
      task.resolve(response.data);
    } else if (response.type === 'error') {
      task.reject(new Error(response.error || 'Worker error'));
    }
    
    // Process next task in queue
    this.processNextTask();
  }
  
  /**
   * Process next task from queue
   */
  private processNextTask(): void {
    if (this.taskQueue.length === 0) return;
    
    // Find an available worker and a task
    for (let i = 0; i < this.numWorkers; i++) {
      const workerIndex = (this.nextWorkerIndex + i) % this.numWorkers;
      
      if (this.workerReady[workerIndex] && this.taskQueue.length > 0) {
        // Get first task
        const task = this.taskQueue.shift()!;
        
        this.workerReady[workerIndex] = false;
        this.nextWorkerIndex = (workerIndex + 1) % this.numWorkers;
        
        // Add to active tasks map (so we can find it when worker responds)
        this.activeTasks.set(task.id, task);
        
        // Note: We clone blocks above, so Transferable doesn't help for inputs
        // But we'll use Transferable for outputs (results) which are only sent once
        // Send task to worker (blocks are already cloned, so no transfer needed)
        this.workers[workerIndex].postMessage(task.message);
        return;
      }
    }
  }
  
  /**
   * Check if WASM is available
   */
  isWASMAvailable(): boolean {
    return this.wasmModule !== null;
  }
  
  /**
   * Process blocks in parallel using persistent workers
   */
  async processBlocksParallel(
    blocks: Uint8Array,
    numBlocks: number,
    key: Uint8Array,
    iv: Uint8Array,
    startBlockNumber: number,
    encrypt: boolean,
    onProgress?: (progress: number) => void
  ): Promise<Uint8Array> {
    if (!this.wasmModule) {
      throw new Error('WASM not initialized');
    }
    
    const startTime = performance.now();
    const BLOCK_SIZE = 32;
    const totalBytes = numBlocks * BLOCK_SIZE;
    
    // Distribute blocks across workers
    const blocksPerWorker = Math.ceil(numBlocks / this.numWorkers);
    const promises: Promise<Uint8Array>[] = [];
    const workerTasks: WorkerTask[] = [];
    
    // Pre-clone key/iv once (they're the same for all workers)
    const workerKey = new Uint8Array(key);
    const workerIv = new Uint8Array(iv);
    
    const cloneStart = performance.now();
    for (let i = 0; i < this.numWorkers; i++) {
      const workerStartBlock = i * blocksPerWorker;
      const workerNumBlocks = Math.min(blocksPerWorker, numBlocks - workerStartBlock);
      
      if (workerNumBlocks <= 0) break;
      
      // Clone blocks for each worker - use slice for efficiency
      const workerBlocksStart = workerStartBlock * BLOCK_SIZE;
      const workerBlocksEnd = (workerStartBlock + workerNumBlocks) * BLOCK_SIZE;
      const workerBlocks = blocks.slice(workerBlocksStart, workerBlocksEnd);
      
      const taskId = `task-${i}-${Date.now()}-${Math.random()}`;
      
      const promise = new Promise<Uint8Array>((resolve, reject) => {
        const task: WorkerTask = {
          id: taskId,
          resolve,
          reject,
          message: {
            type: encrypt ? 'encrypt' : 'decrypt',
            id: taskId,
            blocks: workerBlocks,
            startBlockNumber: startBlockNumber + workerStartBlock,
            numBlocks: workerNumBlocks,
            key: workerKey, // Reuse cloned key
            iv: workerIv,   // Reuse cloned iv
          }
        };
        
        workerTasks.push(task);
        this.taskQueue.push(task);
      });
      
      promises.push(promise);
    }
    const cloneTime = performance.now() - cloneStart;
    console.log(`⏱️ Cloning data: ${cloneTime.toFixed(2)}ms`);
    
    // Process all tasks immediately (they'll be distributed to available workers)
    const queueStart = performance.now();
    while (this.taskQueue.length > 0) {
      this.processNextTask();
    }
    const queueTime = performance.now() - queueStart;
    console.log(`⏱️ Queueing tasks: ${queueTime.toFixed(2)}ms`);
    
    // Wait for all workers to complete
    const workerStart = performance.now();
    const results = await Promise.all(promises);
    const workerTime = performance.now() - workerStart;
    console.log(`⏱️ Worker processing: ${workerTime.toFixed(2)}ms (${(totalBytes / 1024 / 1024 / (workerTime / 1000)).toFixed(2)} MB/s)`);
    
    // Combine results
    const combineStart = performance.now();
    const output = new Uint8Array(totalBytes);
    let offset = 0;
    for (const result of results) {
      output.set(result, offset);
      offset += result.length;
    }
    const combineTime = performance.now() - combineStart;
    console.log(`⏱️ Combining results: ${combineTime.toFixed(2)}ms`);
    
    const totalTime = performance.now() - startTime;
    console.log(`⏱️ Total parallel processing: ${totalTime.toFixed(2)}ms`);
    
    if (onProgress) {
      onProgress(100);
    }
    
    return output;
  }
  
  /**
   * Cleanup workers
   */
  terminate(): void {
    for (const worker of this.workers) {
      worker.terminate();
    }
    this.workers = [];
    this.workerReady = [];
    this.taskQueue = [];
    this.activeTasks.clear();
    if (this.workerUrl) {
      URL.revokeObjectURL(this.workerUrl);
    }
  }
}
