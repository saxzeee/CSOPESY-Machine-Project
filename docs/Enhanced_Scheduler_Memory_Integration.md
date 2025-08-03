# Enhanced Scheduler with Memory-Aware Process Creation

## Overview
This document outlines the comprehensive enhancements made to the CSOPESY scheduler to integrate memory-aware process creation with the demand paging system.

## Key Enhancements

### 1. Memory-Aware Process Creation via `scheduler-start`

#### **Enhanced Process Generation**
- **Memory Allocation**: All processes created via `scheduler-start` now allocate memory within configured constraints
- **Dynamic Memory Sizing**: Random memory allocation using valid sizes (powers of 2)
- **Memory Constraint Adherence**: Respects `min-mem-per-proc` and `max-mem-per-proc` from config.txt
- **Fallback Mechanism**: Attempts smaller memory sizes if allocation fails

#### **Implementation Details**
```cpp
// Enhanced createProcess() method now:
1. Generates random valid memory sizes (powers of 2)
2. Attempts memory allocation through MemoryManager
3. Falls back to smaller sizes if memory pressure exists
4. Creates processes with memory similar to screen -s functionality
```

### 2. Memory Operations in Process Instructions

#### **New Instruction Types**
- **READ Instructions**: `READ 0xADDRESS`
  - Generates random valid memory addresses within process allocation
  - 2-byte aligned addresses for proper memory access
  - Triggers demand paging when pages not resident

- **WRITE Instructions**: `WRITE 0xADDRESS VALUE`
  - Generates random valid memory addresses and values
  - 2-byte aligned addresses for proper memory access
  - Triggers demand paging and page modification

#### **Instruction Generation Weights**
```cpp
Instruction Distribution:
- DECLARE: 15% (variable declarations)
- ADD: 20% (arithmetic operations) 
- SUBTRACT: 15% (arithmetic operations)
- PRINT: 20% (output operations)
- SLEEP: 10% (waiting operations)
- FOR: 10% (loop operations)
- READ: 15% (memory read operations) ← NEW
- WRITE: 15% (memory write operations) ← NEW
```

### 3. Memory Manager API Enhancements

#### **New Public Methods**
- `getMinMemoryPerProcess()`: Returns minimum memory per process constraint
- `getMaxMemoryPerProcess()`: Returns maximum memory per process constraint  
- `isValidMemorySize(size) const`: Public validation for memory sizes
- `getProcessCount()`: Returns number of processes with allocated memory
- `getAllocatedMemorySizes()`: Returns vector of all allocated memory sizes

#### **Enhanced Functionality**
- **Thread-safe validation**: Memory size validation accessible to scheduler
- **Statistics gathering**: Enhanced memory usage tracking
- **Constraint enforcement**: Centralized memory constraint validation

### 4. Intelligent Memory Allocation Strategy

#### **Dynamic Memory Selection**
```cpp
Process Creation Algorithm:
1. Generate list of valid memory sizes (powers of 2)
2. Randomly select from valid range [min_mem, max_mem]
3. Attempt allocation with selected size
4. If allocation fails, try progressively smaller sizes
5. Only create process if memory allocation succeeds
```

#### **Memory Pressure Handling**
- **Graceful degradation**: Falls back to smaller memory allocations
- **Process creation limiting**: Prevents system overcommitment
- **Resource awareness**: Considers available physical memory

### 5. Enhanced Process Instruction Generation

#### **Memory-Aware Address Generation**
```cpp
Address Generation:
- Range: [0, allocatedMemory - 2] 
- Alignment: 2-byte boundaries (address = (address/2)*2)
- Validation: Ensures addresses within process memory space
- Safety: Prevents memory violations during normal operation
```

#### **Realistic Memory Operations**
- **READ operations**: Simulate realistic memory access patterns
- **WRITE operations**: Include both variable and literal value writes
- **Address distribution**: Uniform distribution across process memory space

### 6. Configuration Integration

#### **Memory Configuration Parameters**
```
config.txt parameters now fully integrated:
- min-mem-per-proc: Minimum memory allocation per process
- max-mem-per-proc: Maximum memory allocation per process
- max-overall-mem: Total system memory constraint
- mem-per-frame: Frame size for demand paging
```

#### **Dynamic Range Calculation**
- Automatically calculates valid memory sizes within constraints
- Ensures power-of-2 requirement compliance
- Validates against system memory limits

## Usage Examples

### Basic Usage
```bash
# Initialize system with memory constraints
initialize
scheduler-start

# System will now create processes with:
# - Random memory allocation (64-512 bytes based on config)
# - READ/WRITE instructions included
# - Memory-aware process generation
```

### Memory Configuration Example
```
# config.txt for testing
max-overall-mem 1024      # 1KB total memory
mem-per-frame 64          # 64-byte pages  
min-mem-per-proc 64       # Minimum 64 bytes per process
max-mem-per-proc 512      # Maximum 512 bytes per process

# Results in valid memory sizes: [64, 128, 256, 512]
```

### Expected Behavior
```bash
scheduler-start
# Creates processes with:
# - Memory allocation: 64, 128, 256, or 512 bytes
# - Instructions include READ 0x40 and WRITE 0x80 100
# - Automatic page fault handling
# - Memory pressure management
```

## Technical Benefits

### 1. **Comprehensive Memory Integration**
- All scheduler-generated processes now use memory system
- Seamless integration with demand paging
- Realistic memory access patterns

### 2. **System Resource Management**
- Intelligent memory allocation prevents overcommitment
- Graceful handling of memory pressure
- Dynamic adaptation to available resources

### 3. **Enhanced Testing Capabilities**
- Realistic memory workloads for testing
- Automatic generation of diverse memory operations
- Comprehensive page fault scenario coverage

### 4. **Performance Optimization**
- Memory-aware process creation reduces thrashing
- Balanced memory allocation distribution
- Efficient resource utilization

## Validation and Testing

### Memory Pressure Testing
```bash
# Test with constrained memory
initialize
scheduler-start
vmstat                    # Monitor memory usage
process-smi              # Check per-process allocations
```

### Page Fault Verification
```bash
# Verify READ/WRITE operations trigger page faults
# Check logs/csopesy-backing-store.txt for evictions
# Monitor vmstat for pages-paged-in/out counters
```

### Memory Constraint Compliance
```bash
# Verify all processes respect min/max memory constraints
# Check process-smi output for memory allocations
# Ensure all allocations are powers of 2
```

## Implementation Files Modified

1. **`src/scheduler/scheduler.cpp`**
   - Enhanced `createProcess(name)` with memory allocation
   - Added memory constraint awareness
   - Implemented fallback allocation strategy

2. **`src/process/process.cpp`**
   - Added READ and WRITE instruction generation
   - Enhanced instruction weight distribution
   - Memory-aware address generation

3. **`src/memory/memory_manager.h/.cpp`**
   - Added public memory constraint getters
   - Enhanced API for scheduler integration
   - Added memory allocation statistics

## Future Enhancements

1. **Advanced Memory Allocation Strategies**
   - First-fit, best-fit allocation algorithms
   - Memory fragmentation handling
   - Priority-based memory allocation

2. **Enhanced Memory Operations**
   - Variable-based memory addresses
   - Memory copy operations
   - Shared memory segments

3. **Performance Monitoring**
   - Memory allocation success/failure rates
   - Average memory utilization tracking
   - Process memory usage trends

This comprehensive enhancement transforms the CSOPESY scheduler into a fully memory-aware system that provides realistic memory workloads while maintaining system stability and resource efficiency.
