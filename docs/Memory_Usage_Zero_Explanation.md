# CSOPESY Demand Paging Memory Usage Test 🧪

## 🎯 **Understanding "Used Memory = 0" Behavior**

The **used memory = 0** after `scheduler-start` is **CORRECT** for a demand paging system!

### **Why This Happens**
1. `scheduler-start` creates processes with **virtual memory allocation**
2. No **physical frames** are allocated until memory is actually accessed
3. `getUsedMemory()` counts **physical frames** (real memory usage)
4. Until processes run and access memory → **used memory = 0**

## 🔬 **Step-by-Step Memory Usage Test**

### **Test 1: Verify Virtual Allocation**
```bash
# Start the system
./src/main.exe
initialize

# Run scheduler-start (creates processes with virtual memory)
scheduler-start

# Check memory usage immediately
vmstat
# Expected: Used memory: 0 bytes (NO physical frames allocated yet)

process-smi
# Expected: Shows processes with memory allocations but Used memory: 0
```

### **Test 2: Wait for Process Execution**
```bash
# Wait a few seconds for processes to start executing
# (Processes will execute READ/WRITE instructions)

# Check memory usage after execution starts
vmstat
# Expected: Used memory: > 0 bytes (physical frames now allocated)

process-smi
# Expected: Used memory now shows actual frame usage
```

### **Test 3: Manual Process with Memory Operations**
```bash
# Create a process that immediately accesses memory
screen -c test_memory 256 "WRITE 0x100 42; READ value 0x100; PRINT value"

# Check memory immediately
vmstat
# Expected: Used memory increases (frame allocated for memory access)
```

### **Test 4: Monitor Memory Changes**
```bash
# Before creating process
vmstat
# Note the "Used memory" value

# Create process
screen -c memory_test 128 "DECLARE x 10; WRITE 0x80 x; READ result 0x80"

# After creating but before execution
vmstat
# Should still be same (virtual allocation only)

# Wait for execution to start
# (Process will access memory via WRITE/READ)
screen -ls
vmstat
# Used memory should increase (physical frames allocated)
```

## 🔍 **Expected Memory Behavior Timeline**

### **Phase 1: Process Creation (scheduler-start)**
```
Time: 0s
Action: scheduler-start creates 3 processes (256 bytes each)
Virtual Memory: 768 bytes allocated
Physical Memory: 0 bytes used ← This is what vmstat shows
Reason: No memory accessed yet
```

### **Phase 2: Process Execution Begins**
```
Time: 1-2s
Action: Processes start executing instructions
Virtual Memory: 768 bytes allocated  
Physical Memory: 64-192 bytes used ← vmstat shows increasing usage
Reason: READ/WRITE instructions trigger page faults → frame allocation
```

### **Phase 3: Full Memory Usage**
```
Time: 3-5s
Action: All processes actively running
Virtual Memory: 768 bytes allocated
Physical Memory: 192-512 bytes used ← Maximum usage
Reason: Multiple page faults allocated frames for all active processes
```

### **Phase 4: Process Completion**
```
Time: 10-30s
Action: Processes finish and terminate
Virtual Memory: Decreases as processes finish
Physical Memory: Decreases as frames deallocated ← Memory freed!
Reason: handleProcessCompletion() calls deallocateMemory()
```

## 🚨 **Common Misunderstandings**

### ❌ **Wrong Expectation**
"After scheduler-start, used memory should immediately show process allocations"

### ✅ **Correct Understanding**  
"After scheduler-start, used memory = 0 because no physical memory is used until page faults occur"

### ❌ **Wrong Interpretation**
"Used memory = 0 means the system is broken"

### ✅ **Correct Interpretation**
"Used memory = 0 means demand paging is working correctly - lazy allocation!"

## 🎓 **Educational Value**

### **Real Operating System Concepts**
1. **Virtual Memory**: Process "thinks" it has memory, but OS hasn't allocated it yet
2. **Demand Paging**: Physical memory allocated only when needed
3. **Lazy Allocation**: Efficient memory usage - don't allocate until accessed
4. **Page Faults**: The mechanism that triggers physical memory allocation

### **CSOPESY Implementation Details**
```cpp
// Memory allocation (virtual)
memoryManager->allocateMemory(processName, memorySize);  // Updates processMemoryMap

// Memory usage (physical) - only happens on access
process->executeNextInstruction(); // Contains WRITE 0x100 42
// → Page fault → handlePageFault() → Frame allocated → getUsedMemory() increases
```

## 🎯 **Verification Commands**

### **Quick Memory Test**
```bash
# Terminal 1: Monitor memory
while true; do clear; vmstat; sleep 1; done

# Terminal 2: Create activity
initialize
scheduler-start
# Watch Terminal 1 - used memory should increase after ~2-3 seconds
```

### **Detailed Process Tracking**
```bash
initialize
vmstat  # Baseline
scheduler-start
vmstat  # Should be same (virtual allocation)
sleep 5
vmstat  # Should show increased usage (physical allocation)
screen -ls  # Check process states
```

## 🔧 **If Used Memory NEVER Increases**

If memory usage stays at 0 even after processes run for several seconds, check:

1. **Are processes actually executing?**
   ```bash
   screen -ls  # Check if processes are running
   ```

2. **Do processes have memory operations?**
   ```bash
   screen -r process1  # View process logs for READ/WRITE operations
   ```

3. **Is instruction generation working?**
   ```bash
   # Processes should have READ/WRITE instructions mixed with others
   ```

4. **Are page faults occurring?**
   ```bash
   vmstat  # Check "Num paged in" counter - should increase
   ```

## 🎉 **Expected Results**

✅ **Used memory = 0** immediately after `scheduler-start` (CORRECT!)  
✅ **Used memory > 0** after processes execute for 2-3 seconds  
✅ **Used memory fluctuates** as processes complete and memory is freed  
✅ **Paged in/out counters** increase as page faults occur  

This behavior demonstrates **authentic demand paging** - exactly how real operating systems work! 🚀
