# Memory Constraint Test for MO2 Specification Compliance

## Test Configuration
Your `config.txt` should have these exact settings:
```
num-cpu 2
scheduler "fcfs"
quantum-cycles 0
batch-process-freq 1
min-ins 4000
max-ins 4000
delay-per-exec 0
max-overall-mem 512
mem-per-frame 256
min-mem-per-proc 512
max-mem-per-proc 512
```

## Expected Behavior Analysis

### Memory Calculation:
- **Total Memory**: 512 bytes
- **Frame Size**: 256 bytes → **2 total frames**
- **Process Requirements**: 512 bytes each → **2 frames per process**
- **Result**: Only **1 process** can fit in memory

### Fixed Implementation:
1. **Memory Manager**: Now checks total virtual memory allocations vs. physical memory
2. **Scheduler**: Stops creating processes when memory allocation fails
3. **Process Creation**: Fails gracefully when insufficient memory

## Test Steps

### Step 1: Manual Process Creation Test
```bash
# Start system
.\src\main.exe
initialize

# Try to create first process (should succeed)
screen -s process1 512
echo "First process should be created successfully"

# Try to create second process (should fail)
screen -s process2 512
echo "Second process should fail - insufficient memory"

# Check status
process-smi
screen -ls
```

### Step 2: Scheduler-Start Test (Video Required)
```bash
# Start fresh system
.\src\main.exe
initialize

# Start scheduler (will try to create multiple processes)
scheduler-start

# Wait 5 seconds
# (Only one process should be running)

# Check process list
screen -ls

# Check memory usage
process-smi
```

### Expected Results:
1. **Process Creation**: Only 1 process should be created
2. **Memory Usage**: 512 bytes allocated (100% memory utilization)
3. **Process State**: 1 process running, no additional processes created
4. **Scheduler Behavior**: Stops attempting to create processes after memory failure

## Verification Commands

### Check Memory Allocation:
```bash
process-smi
# Expected output:
# Running processes and memory usage:
# ------------------------------------------
# process1                     512 bytes
# ------------------------------------------
# Total memory: 512 bytes
# Used memory: XXX bytes (depends on page faults)
```

### Check Process Status:
```bash
screen -ls
# Expected: Only 1 process listed
```

### Check System Status:
```bash
vmstat
# Expected: Memory fully allocated, no free memory for new processes
```

## Implementation Changes Made

### 1. Memory Manager Fix (`src/memory/memory_manager.cpp`):
```cpp
// Added physical memory constraint check
size_t totalVirtualMemoryAllocated = 0;
for (const auto& pair : processMemoryMap) {
    totalVirtualMemoryAllocated += pair.second.allocatedMemory;
}

// Fail allocation if it would exceed physical memory
if (totalVirtualMemoryAllocated + requiredMemory > maxOverallMemory) {
    return false;
}
```

### 2. Scheduler Fix (`src/scheduler/scheduler.cpp`):
```cpp
// Stop creating processes when memory allocation fails
for (int i = 0; i < processesToCreate; ++i) {
    if (!createProcess()) {
        // Memory allocation failed - stop creating more processes
        break;
    }
}
```

This ensures that the system respects physical memory constraints and only creates processes that can actually fit in available memory, exactly as required by the MO2 specification test case.
