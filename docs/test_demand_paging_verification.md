# Demand Paging Allocator Verification Test

## Test Objective
Verify that the CSOPESY implementation fully satisfies all demand paging allocator requirements:
1. Pages loaded into physical memory frames on demand
2. Page fault handling when accessing non-resident pages
3. Page replacement algorithm when no free frames available
4. Backing store operations for page eviction and restoration

## Test Configuration
Ensure `config.txt` has memory-constrained settings to trigger page faults:
```
num-cpu 4
scheduler "rr"
quantum-cycles 5
max-overall-mem 1024    # Small memory to force page faults
mem-per-frame 64        # 64-byte pages
min-mem-per-proc 64
max-mem-per-proc 512
```

## Test Case 1: Basic Demand Paging Verification

### Purpose
Verify that pages are loaded only when accessed, not at allocation time.

### Steps
```bash
# 1. Initialize system
initialize
scheduler-start

# 2. Create process with memory allocation (should not load pages yet)
screen -c demand_test1 256

# 3. Check initial memory state
vmstat
# Expected: Low pages-paged-in count, process allocated but pages not loaded

# 4. Access the process to trigger page loading
screen -r demand_test1
# Enter some commands to access memory
DECLARE var1 100
exit

# 5. Check memory state after access
vmstat
# Expected: pages-paged-in count increased, showing on-demand loading
```

### Expected Results
- **Before access**: Process allocated but no physical pages loaded
- **After access**: Pages loaded only when memory operations occur
- **vmstat output**: Shows increase in pages-paged-in counter

## Test Case 2: Page Fault Handling Verification

### Purpose
Verify automatic page fault detection and resolution during memory operations.

### Steps
```bash
# 1. Start fresh system
initialize
scheduler-start

# 2. Create process with complex memory operations
screen -c pagefault_test 512 "DECLARE varA 10; DECLARE varB 20; WRITE 0x200 varA; READ varC 0x200; PRINT(\"Value: \" + varC)"

# 3. Monitor process execution
screen -ls
# Process should execute despite page faults

# 4. Check memory statistics
vmstat
process-smi
# Expected: Page fault counts, successful memory operations
```

### Expected Results
- **Page fault detection**: Automatic detection when pages not resident
- **Transparent handling**: Operations complete successfully after page loading
- **Process continuation**: No interruption to process execution flow

## Test Case 3: Page Replacement Algorithm Verification

### Purpose
Verify page replacement when physical memory is exhausted.

### Steps
```bash
# 1. Initialize with very limited memory
initialize
scheduler-start

# 2. Create multiple processes to fill physical memory
screen -c proc1 256 "DECLARE var1 100; WRITE 0x100 var1"
screen -c proc2 256 "DECLARE var2 200; WRITE 0x200 var2"
screen -c proc3 256 "DECLARE var3 300; WRITE 0x300 var3"
screen -c proc4 256 "DECLARE var4 400; WRITE 0x400 var4"

# 3. Create additional process to force page replacement
screen -c proc5 256 "DECLARE var5 500; WRITE 0x500 var5"

# 4. Check backing store activity
# Open logs/csopesy-backing-store.txt to verify evicted pages
# Expected: EVICTED entries with process and page information

# 5. Access earlier processes to trigger page restoration
screen -r proc1
READ value1 0x100
PRINT("Value: " + value1)
exit

# 6. Verify statistics
vmstat
# Expected: Both pages-paged-in and pages-paged-out counters increased
```

### Expected Results
- **Frame exhaustion**: System detects when no free frames available
- **Victim selection**: FIFO algorithm selects pages for eviction
- **Page eviction**: Selected pages written to backing store
- **Page restoration**: Evicted pages loaded back when accessed

## Test Case 4: Backing Store Operations Verification

### Purpose
Verify backing store file operations and data integrity.

### Steps
```bash
# 1. Start system and create processes
initialize
scheduler-start

# 2. Fill memory and force evictions
screen -c store_test1 512 "DECLARE big_var 1000; WRITE 0x400 big_var"
screen -c store_test2 512 "DECLARE another_var 2000; WRITE 0x500 another_var"
screen -c store_test3 512 "DECLARE third_var 3000; WRITE 0x600 third_var"

# 3. Examine backing store file
# Check logs/csopesy-backing-store.txt content
# Expected format:
# EVICTED: Process=<processId> Page=<pageNumber> Frame=<frameNumber>
# <hexadecimal data in 16-byte rows>

# 4. Verify page restoration maintains data integrity
screen -r store_test1
READ restored_value 0x400
PRINT("Restored: " + restored_value)
# Expected: Original value (1000) correctly restored
```

### Expected Results
- **Backing store file**: Contains evicted page entries with metadata
- **Data format**: Hexadecimal representation of page contents
- **Data integrity**: Restored pages contain original data
- **Accessibility**: Text file can be examined independently

## Test Case 5: Memory Violation vs Page Fault Distinction

### Purpose
Verify system correctly distinguishes between page faults and memory violations.

### Steps
```bash
# 1. Initialize system
initialize
scheduler-start

# 2. Create process with valid operations (should trigger page faults, not violations)
screen -c valid_access 256 "DECLARE var 100; WRITE 0x80 var; READ result 0x80"

# 3. Create process with invalid operations (should trigger memory violations)
screen -c invalid_access 256 "WRITE 0x1000 100"  # Beyond allocated memory

# 4. Check process states
screen -ls
process-smi

# 5. Verify different handling
# valid_access: Should execute successfully with page faults
# invalid_access: Should terminate with memory violation
```

### Expected Results
- **Valid access**: Page faults handled transparently, process continues
- **Invalid access**: Memory violation detected, process terminated
- **Clear distinction**: Different error handling for page faults vs violations

## Validation Checklist

### ✅ Demand Paging Requirements
- [ ] **On-demand loading**: Pages loaded only when accessed
- [ ] **Page fault detection**: Automatic detection of non-resident pages
- [ ] **Transparent handling**: Operations retry after page fault resolution
- [ ] **Frame management**: Free frame tracking and allocation
- [ ] **Page replacement**: FIFO algorithm when frames exhausted
- [ ] **Backing store**: Text file storage for evicted pages
- [ ] **Data integrity**: Restored pages maintain original content
- [ ] **Statistics tracking**: Accurate paging statistics in vmstat

### 📋 Success Criteria
1. **vmstat command** shows:
   - Increasing pages-paged-in when pages loaded
   - Increasing pages-paged-out when pages evicted
   - Accurate free/used memory statistics

2. **process-smi command** shows:
   - Per-process memory allocation
   - Memory usage statistics

3. **Backing store file** contains:
   - EVICTED entries with process metadata
   - Hexadecimal page data
   - Readable text format

4. **Process execution**:
   - No interruption during page faults
   - Successful completion of memory operations
   - Proper distinction between page faults and violations

## Additional Monitoring Commands

```bash
# Monitor system state
vmstat                    # Memory statistics and page fault counts
process-smi              # Per-process memory usage
screen -ls               # Process states and execution status

# Examine backing store
# Open logs/csopesy-backing-store.txt in text editor
# Verify EVICTED entries and hexadecimal page data

# Check memory reports
# Use any memory reporting commands if available
```

## Notes
- Small memory configuration (1024 bytes) ensures page faults occur
- 64-byte frames create realistic paging scenarios
- Multiple processes stress-test frame allocation
- Text-based backing store allows manual verification
- Clear distinction between page faults and memory violations

This comprehensive test verifies all aspects of the demand paging allocator implementation.
