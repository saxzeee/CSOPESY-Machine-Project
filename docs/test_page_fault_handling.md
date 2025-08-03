# Testing Scheduler and Memory Interaction

## Page Fault Handling Implementation

### Key Features Implemented:

1. **Instruction-Level Page Fault Handling**
   - Instructions can only execute when valid pages are resident in memory
   - Page faults trigger automatic page loading from backing store
   - Instructions are retried after page fault resolution

2. **Symbol Table Page Management**
   - Variable operations ensure symbol table segment is in memory
   - DECLARE, ADD, SUBTRACT, PRINT operations check symbol table availability

3. **Memory Access Page Management**
   - READ/WRITE operations ensure target memory pages are resident
   - Continuous page fault handling until valid pages are found

## Test Scenarios

### Scenario 1: Symbol Table Page Fault
```bash
# Initialize system
initialize
scheduler-start

# Create process with variable operations that may trigger symbol table page faults
screen -c symbol_test 256 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; PRINT(\"Result: \" + varA)"
```

**Expected Behavior:**
- Each variable operation checks if symbol table page is resident
- If not resident, triggers page fault and retries instruction
- Process continues after page is loaded

### Scenario 2: Memory Access Page Fault
```bash
# Create process with memory operations that may trigger page faults
screen -c memory_test 512 "DECLARE varA 10; WRITE 0x1200 varA; READ varC 0x1200; PRINT(\"Value: \" + varC)"
```

**Expected Behavior:**
- WRITE 0x1200 ensures page containing address 0x1200 is resident
- READ 0x1200 ensures the same page is still resident
- Instructions retry automatically on page faults

### Scenario 3: Complex Operations with Multiple Page Faults
```bash
# Create process similar to the example scenario
screen -c faulty_process 256 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x1100 varA; READ varC 0x1100; PRINT(\"Result: \" + varC)"
```

**Expected Behavior:**
1. DECLARE operations ensure symbol table is resident
2. ADD operation ensures symbol table is resident for variable access
3. WRITE 0x1100 ensures target page is resident
4. READ 0x1100 ensures target page is still resident
5. PRINT ensures symbol table is resident for variable access
6. Each operation retries on page fault until successful

## Implementation Details

### Page Fault Detection
- Methods return "Page fault occurred while accessing memory. Retrying..." for memory page faults
- Methods return "Page fault occurred while accessing symbol table. Retrying..." for symbol table page faults

### Instruction Retry Mechanism
- `executeNextInstruction()` detects page fault messages
- Instructions remain in queue when page fault occurs
- Process retries the same instruction after page fault handling

### Memory Manager Integration
- `ensurePageResident()` method handles page fault resolution
- Automatic victim page selection using FIFO algorithm
- Backing store operations for page swapping

### Real-World OS Behavior Simulation
- Page faults occur only when process is assigned CPU worker
- Continuous retry until valid pages are found
- No instruction advancement during page fault handling

## Monitoring Page Fault Activity

Use these commands to monitor page fault behavior:
- `vmstat` - Shows page fault counts and statistics
- `process-smi` - Shows memory usage per process  
- `backing-store` - Shows backing store activity

## Configuration for Testing

Ensure config.txt has appropriate memory settings to trigger page faults:
```
max-overall-mem 1024
mem-per-frame 256
min-mem-per-proc 64
max-mem-per-proc 512
```

Small memory settings increase the likelihood of page faults for testing purposes.
