# CSOPESY MO2 Test Run Script

## Basic Test Sequence

### 1. Initialize System
```
initialize
```

### 2. Check Memory Status
```
process-smi
vmstat
```

### 3. Create Process with Memory Allocation
```
screen -s testproc 256
```

### 4. Create Process with Custom Instructions
```
screen -c calculator 512 "DECLARE x 10; DECLARE y 5; ADD x x y; PRINT(\"Result: \" + x)"
```

### 5. Test Memory Operations
```
screen -c memtest 256 "DECLARE val 42; WRITE 0x100 val; READ result 0x100; PRINT(\"Value: \" + result)"
```

### 6. Test Memory Violation
```
screen -c violation 128 "WRITE 0x200 99"
screen -r violation
```

### 7. Check Memory Status Again
```
process-smi
vmstat
```

### 8. Test Invalid Memory Allocation
```
screen -s invalid 100
screen -s invalid2 32
```

### 9. Test Invalid Instruction Count
```
screen -c toomany 256 ""
```

### 10. Exit
```
exit
```

## Expected Results

1. **Initialize**: System loads with memory configuration displayed
2. **process-smi**: Shows initial memory state (0 processes, all memory free)
3. **vmstat**: Shows detailed memory statistics with zero page operations
4. **testproc creation**: Process created successfully with 256 bytes
5. **calculator**: Process created with custom instructions
6. **memtest**: Process demonstrates READ/WRITE operations
7. **violation**: Process terminates with memory access violation
8. **Status check**: Shows updated memory usage and statistics
9. **Invalid tests**: Show appropriate error messages
10. **Exit**: Program terminates cleanly

This test sequence validates all major MO2 features including memory management, custom instructions, error handling, and system monitoring.
