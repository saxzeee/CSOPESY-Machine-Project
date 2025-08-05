# CSOPESY MO2 Test Cases

## Basic Functionality Tests

### 1. System Initialization
**Test Case**: Initialize system with updated config
**Commands**:
```
initialize
```
**Expected Result**: System loads with memory configuration displayed

### 2. Memory Allocation Validation
**Test Case**: Valid memory sizes (powers of 2, 64-65536 bytes)
**Commands**:
```
screen -s test1 64
screen -s test2 128
screen -s test3 256
screen -s test4 1024
screen -s test5 65536
```
**Expected Result**: All processes created successfully

### 3. Invalid Memory Allocation
**Test Case**: Invalid memory sizes
**Commands**:
```
screen -s invalid1 32
screen -s invalid2 100
screen -s invalid3 70000
```
**Expected Result**: "Invalid memory allocation" message for each

### 4. Custom Instructions Processing
**Test Case**: Create process with custom instructions
**Commands**:
```
screen -c proc1 256 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; PRINT(\"Result: \" + varA)"
```
**Expected Result**: Process created with 4 instructions

### 5. Memory Operations Testing
**Test Case**: READ and WRITE instructions
**Commands**:
```
screen -c memtest 512 "DECLARE x 42; WRITE 0x100 x; READ y 0x100; PRINT(\"Value: \" + y)"
```
**Expected Result**: Process executes memory operations successfully

### 6. Memory Access Violation
**Test Case**: Access outside allocated memory
**Commands**:
```
screen -c violation 256 "WRITE 0x500 42"
screen -r violation
```
**Expected Result**: Process terminates with memory violation error

### 7. Instruction Count Validation
**Test Case**: Invalid instruction count (outside 1-50 range)
**Commands**:
```
screen -c toomany 256 ""
screen -c toomany2 256 "PRINT(\"1\"); PRINT(\"2\"); ... (51 instructions)"
```
**Expected Result**: "Invalid command" message

## Memory Management Tests

### 8. Process Memory Report
**Test Case**: View memory usage with process-smi
**Commands**:
```
screen -s proc1 256
screen -s proc2 512
process-smi
```
**Expected Result**: Memory overview with running processes listed

### 9. Detailed Memory Statistics
**Test Case**: View detailed memory stats with vmstat
**Commands**:
```
vmstat
```
**Expected Result**: Detailed memory and CPU statistics displayed

### 10. Symbol Table Overflow
**Test Case**: Exceed 32 variable limit (64 bytes)
**Commands**:
```
screen -c overflow 256 "DECLARE v1 1; DECLARE v2 2; ... (33+ variables)"
```
**Expected Result**: After 32 variables, additional declarations ignored

### 11. Page Fault Simulation
**Test Case**: Force page faults with memory pressure
**Commands**:
```
(Create multiple processes with large memory allocations to fill physical memory)
screen -s big1 4096
screen -s big2 4096
screen -s big3 4096
screen -s big4 4096
```
**Expected Result**: Page faults handled, backing store operations occur

## Process Management Tests

### 12. Process Screen Access
**Test Case**: Access process with memory info
**Commands**:
```
screen -s testproc 512
screen -r testproc
```
**Expected Result**: Process screen shows memory allocation info

### 13. Memory Violation Recovery
**Test Case**: Handle process termination due to memory violation
**Commands**:
```
screen -c badproc 128 "WRITE 0x200 99"
(wait for execution)
screen -r badproc
```
**Expected Result**: Error message with timestamp and invalid address

### 14. Mixed Instruction Types
**Test Case**: Combine all instruction types
**Commands**:
```
screen -c mixed 1024 "DECLARE a 5; DECLARE b 10; ADD a a b; WRITE 0x100 a; READ c 0x100; PRINT(\"Sum: \" + c)"
```
**Expected Result**: All instructions execute correctly

## Stress Tests

### 15. Maximum Memory Usage
**Test Case**: Use all available memory
**Commands**:
```
(Create processes until memory full)
process-smi
vmstat
```
**Expected Result**: Memory usage at maximum, backing store active

### 16. Rapid Process Creation
**Test Case**: Create many processes quickly
**Commands**:
```
scheduler-test
(observe automatic process generation with memory allocation)
```
**Expected Result**: System handles multiple processes with memory management

### 17. Backing Store Operations
**Test Case**: Verify backing store file operations
**Commands**:
```
(Check logs/csopesy-backing-store.txt after page evictions)
```
**Expected Result**: Backing store file contains evicted page data

## Error Handling Tests

### 18. Uninitialized System
**Test Case**: Try commands before initialization
**Commands**:
```
process-smi
vmstat
screen -s test 256
```
**Expected Result**: "Please initialize the system first" for each

### 19. Invalid Process Names
**Test Case**: Access non-existent processes
**Commands**:
```
screen -r nonexistent
```
**Expected Result**: "Process nonexistent not found"

### 20. Malformed Instructions
**Test Case**: Invalid instruction syntax
**Commands**:
```
screen -c bad 256 "INVALID_INSTRUCTION; MALFORMED SYNTAX"
```
**Expected Result**: Instructions execute with error handling

## Integration Tests

### 21. Full Workflow Test
**Test Case**: Complete system workflow
**Commands**:
```
initialize
process-smi
screen -s worker1 256
screen -c worker2 512 "DECLARE x 100; WRITE 0x200 x; READ y 0x200; PRINT(\"Value: \" + y)"
scheduler-start
vmstat
screen -r worker2
```
**Expected Result**: Complete workflow executes successfully

### 22. Memory Cleanup Test
**Test Case**: Verify memory deallocation on process termination
**Commands**:
```
screen -s temp 1024
(wait for process completion)
process-smi
```
**Expected Result**: Memory freed after process termination

## Performance Tests

### 23. Page Replacement Algorithm
**Test Case**: Test page replacement under memory pressure
**Commands**:
```
(Create processes with overlapping memory access patterns)
vmstat
```
**Expected Result**: Page in/out counters increase appropriately

### 24. CPU Utilization Tracking
**Test Case**: Monitor CPU tick counters
**Commands**:
```
scheduler-start
vmstat
(observe tick counters over time)
```
**Expected Result**: CPU ticks tracked accurately

### 25. Concurrent Memory Access
**Test Case**: Multiple processes accessing memory simultaneously
**Commands**:
```
screen -c concurrent1 256 "WRITE 0x100 50; READ x 0x100"
screen -c concurrent2 256 "WRITE 0x150 75; READ y 0x150"
```
**Expected Result**: Concurrent memory operations handled safely
