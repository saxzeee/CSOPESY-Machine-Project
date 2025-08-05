# 🎯 PROCESS FILTERING FIX FOR MEMORY DEADLOCK SCENARIO

## **Issue Identified**
Your groupmate correctly identified that `process-smi` was showing process `p001` in the memory deadlock scenario, which shouldn't appear if the deadlock has terminated all processes or prevented them from running.

## **Problem Analysis**
```
==========================================
| CSOPESY Process and Memory Monitor     |
==========================================
CPU-Util: 0.0%
Memory-Util: 200.0%
Memory: 32768 / 16384 bytes
==========================================
Running processes and memory usage:
------------------------------------------
p001                     32768 bytes    ← This shouldn't show in deadlock
------------------------------------------
```

**Why this was happening:**
- The memory manager was showing ALL processes that had memory allocated
- It wasn't checking if those processes were actually still running/active
- In a memory deadlock scenario, processes should be terminated or stuck

## **Solution Applied**

### **Added Process State Filtering:**
✅ **Check against running processes list** - Only show processes that are in the active processes list  
✅ **Exclude terminated processes** - Filter out processes with `ProcessState::TERMINATED`  
✅ **Display "No active processes"** - Show appropriate message when all processes are terminated  
✅ **Maintain memory statistics** - Keep accurate memory utilization calculations  

### **New Logic:**
```cpp
// Show only processes that are currently running/active
bool hasActiveProcesses = false;
for (const auto& pair : processMemoryMap) {
    // Check if this process is in the running processes list
    bool isActiveProcess = false;
    for (const auto& process : runningProcesses) {
        if (process != nullptr && process->name == pair.first) {
            // Only show processes that are not terminated
            if (process->state != ProcessState::TERMINATED) {
                isActiveProcess = true;
                break;
            }
        }
    }
    
    if (isActiveProcess) {
        // Display the process
        hasActiveProcesses = true;
    }
}

if (!hasActiveProcesses) {
    std::cout << "No active processes found." << std::endl;
}
```

## **Expected Results**

### **Before Fix (Incorrect):**
```
Running processes and memory usage:
------------------------------------------
p001                     32768 bytes
------------------------------------------
```

### **After Fix (Correct for Deadlock):**
```
Running processes and memory usage:
------------------------------------------
No active processes found.
------------------------------------------
```

### **After Fix (With Active Processes):**
```
Running processes and memory usage:
------------------------------------------
activeProcess1           16384 bytes
activeProcess2            8192 bytes
------------------------------------------
```

## **Memory Deadlock Scenario Benefits**

1. **Accurate Process Display** - Only shows processes that are genuinely active
2. **Proper Deadlock Demonstration** - Shows "No active processes" when deadlock terminates all processes
3. **Clean Output** - Eliminates misleading process information
4. **Educational Value** - Better demonstrates the deadlock concept

## **Testing the Fix**

Run your memory deadlock test sequence:
```bash
.\src\main.exe
scheduler-test
# (wait 5 seconds)
scheduler-stop
process-smi    # ← Should now show "No active processes found"
vmstat
```

The `process-smi` command will now properly reflect the system state during memory deadlock, showing only truly active processes or "No active processes found" when the deadlock has terminated all processes.

Your groupmate's suggestion was spot-on - this filtering condition makes the memory deadlock demonstration much more accurate and educational!
