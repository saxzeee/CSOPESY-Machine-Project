# Corrected Memory Deadlock Test Sequence

## Issue Analysis
The program crashes when running `process-smi` and `vmstat` commands because the system is not properly initialized. The commands check for initialization status and the memory manager/scheduler must be set up first.

## Root Cause
Looking at the command processor code:
- Both `process-smi` and `vmstat` commands check `if (!initialized)` 
- If not initialized, they should return early with a message
- However, if the system is partially initialized or there are null pointers, it can cause crashes

## Corrected Test Sequence

1. **First, initialize the system:**
   ```
   initialize
   ```
   OR
   ```
   initialize config.txt
   ```

2. **Then run the scheduler test:**
   ```
   scheduler-test
   ```

3. **Wait for 5 seconds** (let processes build up and memory pressure increase)

4. **Stop the scheduler:**
   ```
   scheduler-stop
   ```

5. **Run process-smi every 2 seconds for 10 seconds (5 times):**
   ```
   process-smi
   (wait 2 seconds)
   process-smi
   (wait 2 seconds)
   process-smi
   (wait 2 seconds)
   process-smi
   (wait 2 seconds)
   process-smi
   ```

6. **Run vmstat:**
   ```
   vmstat
   ```

## Expected Behavior
- After initialization, the memory manager and scheduler will be properly set up
- The commands should not crash and will display memory statistics
- You should observe the memory deadlock scenario as described

## Additional Safety Checks
If the crash still occurs, it might be due to:

1. **Null pointer access in memory manager** - Check if `getMemoryManager()` returns null
2. **Invalid memory access in reporting functions** - The memory reporting might be accessing deallocated memory
3. **Thread safety issues** - If scheduler is running in background, concurrent access might cause issues

## Debug Steps
1. Add debug output in the command processor to verify initialization state
2. Add null pointer checks before calling memory manager methods
3. Ensure proper synchronization in memory reporting functions
