# Testing Enhanced Scheduler with Memory Operations

## Quick Test Guide

### 1. **Configure for Memory-Constrained Testing**
Ensure your `config.txt` has these settings:
```
num-cpu 2
scheduler "rr"
quantum-cycles 5
batch-process-freq 1
min-ins 100
max-ins 200
delay-per-exec 0
max-overall-mem 1024    # Small memory to force paging
mem-per-frame 64        # 64-byte pages
min-mem-per-proc 64     # Minimum process memory
max-mem-per-proc 512    # Maximum process memory
```

### 2. **Test Enhanced scheduler-start**
```bash
# Start CSOPESY
./src/main.exe

# Initialize and start enhanced scheduler
initialize
scheduler-start

# Monitor the results
vmstat                  # Shows memory usage and paging activity
process-smi            # Shows individual process memory allocations
screen -ls             # Lists processes with their memory allocations
```

### 3. **Expected Enhancements**

#### **Memory Allocation**
- All processes now have memory allocations (64, 128, 256, or 512 bytes)
- Memory sizes are powers of 2 within configured constraints
- Process creation respects available memory

#### **New Instructions**
You should see processes executing:
```
READ 0x40              # Reading from memory address 0x40
WRITE 0x80 150         # Writing value 150 to address 0x80
READ 0x120             # Reading from memory address 0x120
WRITE 0x200 75         # Writing value 75 to address 0x200
```

#### **Memory Statistics**
- `vmstat` shows pages-paged-in and pages-paged-out counters
- `process-smi` displays memory allocation per process
- Page faults handled automatically during READ/WRITE operations

### 4. **Test Memory Pressure**
```bash
# Let scheduler create many processes to fill memory
# Wait for a few minutes to see page replacement in action

# Check backing store activity
# (Open logs/csopesy-backing-store.txt to see evicted pages)

# Verify continuous operation despite memory pressure
```

### 5. **Validation Checklist**

✅ **Memory Integration**
- [ ] All scheduler-created processes have memory allocations
- [ ] Memory sizes are powers of 2 (64, 128, 256, 512)
- [ ] Memory allocations respect min/max constraints

✅ **Memory Operations**
- [ ] READ instructions appear in process logs
- [ ] WRITE instructions appear in process logs  
- [ ] Memory addresses are within allocated ranges
- [ ] Instructions execute without violations

✅ **System Behavior**
- [ ] vmstat shows paging activity (pages-paged-in/out)
- [ ] Backing store file contains evicted page data
- [ ] System handles memory pressure gracefully
- [ ] Process creation slows/stops when memory full

✅ **Enhanced Monitoring**
- [ ] process-smi shows memory allocations
- [ ] screen -ls displays process memory info
- [ ] vmstat reports accurate memory statistics

### 6. **Sample Output**

#### Expected process-smi output:
```
Running processes and memory usage:
------------------------------------------
process1                     256 bytes
process2                     128 bytes  
process3                     512 bytes
process4                      64 bytes
------------------------------------------
```

#### Expected vmstat output:
```
total memory: 1024 bytes
used memory: 960 bytes
free memory: 64 bytes
pages paged in: 45
pages paged out: 12
```

#### Expected process logs:
```
Process name: process1
Instruction: Line 15 / 150
Created at: 14:25:32
Memory: 256 bytes

--- Process Logs ---
DECLARE(x, 42)
READ 0x80 = 0
WRITE 0x120 42
ADD(sum, x, 10)
READ 0x40 = 42
PRINT("Hello world from process1!" + sum)
```

This enhanced system now provides realistic memory workloads while maintaining the demand paging functionality!
