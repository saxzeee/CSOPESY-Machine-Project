# CSOPESY MO2 Implementation Summary

## Overview
Successfully implemented MO2 (Machine Organization 2) enhancements to the existing CSOPESY operating system simulator. The implementation adds comprehensive memory management, demand paging, and advanced process monitoring capabilities.

## Implementation Changes Table

| Component | File | Changes Made | Purpose |
|-----------|------|--------------|---------|
| **Configuration** | `src/config/config.h` | Added memory parameters: `maxOverallMemory`, `memoryPerFrame`, `minMemoryPerProcess`, `maxMemoryPerProcess` | Support memory configuration |
| **Configuration** | `src/config/config.cpp` | Added parsing for new memory parameters in config file | Load memory settings from config.txt |
| **Configuration** | `config.txt` | Added memory parameters: `max-overall-mem`, `mem-per-frame`, `min-mem-per-proc`, `max-mem-per-proc` | Configure system memory limits |
| **Memory Manager** | `src/memory/memory_manager.h` | Complete rewrite: Added frame management, page tables, backing store, statistics tracking | Core memory management functionality |
| **Memory Manager** | `src/memory/memory_manager.cpp` | Implemented demand paging, page fault handling, memory allocation/deallocation, backing store operations | Memory virtualization and paging |
| **Process** | `src/process/process.h` | Added memory-related fields: `allocatedMemory`, `baseAddress`, memory violation tracking, new constructors | Process memory integration |
| **Process** | `src/process/process.cpp` | Added memory-aware constructors, READ/WRITE instruction processing, memory violation handling | Memory instruction support |
| **Scheduler** | `src/scheduler/scheduler.h` | Added MemoryManager integration, new createProcess overloads | Memory-aware process creation |
| **Scheduler** | `src/scheduler/scheduler.cpp` | Initialize MemoryManager, implement memory-aware process creation methods | Scheduler-memory integration |
| **Command Processor** | `src/commands/command_processor.h` | Added helper methods for instruction parsing and memory validation | Enhanced command handling |
| **Command Processor** | `src/commands/command_processor.cpp` | Added `process-smi`, `vmstat` commands; enhanced `screen` command with memory parameters; updated help | New memory commands |

## New Features Implemented

### 1. Memory Management System
- **Demand Paging**: Pages loaded into physical memory frames on demand
- **Page Fault Handling**: Automatic page loading from backing store
- **Page Replacement**: LRU-style victim frame selection when memory full
- **Backing Store**: File-based storage for evicted pages (`logs/csopesy-backing-store.txt`)
- **Memory Validation**: Power-of-2 memory sizes between 64-65536 bytes

### 2. Enhanced Commands
- **`process-smi`**: GPU-style memory and process overview
- **`vmstat`**: Detailed memory statistics (total, used, free memory, CPU ticks, page statistics)
- **`screen -s <name> <memory>`**: Create process with specific memory allocation
- **`screen -c <name> <memory> "<instructions>"`**: Create process with custom instructions (1-50)
- **Enhanced `screen -r`**: Shows memory violation errors with timestamps

### 3. Memory Instructions
- **`READ <var> <hex_addr>`**: Read uint16 value from memory address to variable
- **`WRITE <hex_addr> <value>`**: Write uint16 value to memory address
- **Symbol Table**: 64-byte segment for up to 32 uint16 variables per process
- **Memory Violation Detection**: Automatic process termination on invalid access

### 4. Process Memory Features
- **Memory Allocation**: Per-process memory allocation with validation
- **Address Space**: Virtual memory addressing with page table management
- **Memory Violation Tracking**: Timestamp and address logging for violations
- **Custom Instruction Support**: Semicolon-separated instruction sequences

### 5. System Monitoring
- **Memory Usage Tracking**: Real-time monitoring of allocated/free memory
- **CPU Statistics**: Idle and active CPU tick counters
- **Page Statistics**: Pages paged in/out counters
- **Process Memory Display**: Per-process memory usage in reports

## Key Technical Features

### Memory Architecture
- **Frame-based Memory**: Fixed-size frames for efficient management
- **Page Table Management**: Virtual-to-physical address translation
- **Concurrent Access**: Thread-safe memory operations with mutex protection
- **Memory Cleanup**: Automatic deallocation on process termination

### Error Handling
- **Invalid Memory Sizes**: Validation and error messages
- **Memory Violations**: Process termination with detailed error info
- **Instruction Limits**: 1-50 instruction validation for custom processes
- **Initialization Checks**: Commands blocked until system initialized

### Performance Optimizations
- **Efficient Page Replacement**: Quick victim frame selection
- **Memory Statistics**: Real-time tracking without performance impact
- **Background Operations**: Non-blocking backing store operations
- **Resource Management**: Proper cleanup and resource deallocation

## Configuration Example

```
num-cpu 16
scheduler "rr"
quantum-cycles 5
batch-process-freq 1
min-ins 5000
max-ins 5000
delay-per-exec 0
max-overall-mem 16384
mem-per-frame 64
min-mem-per-proc 64
max-mem-per-proc 1024
```

## Usage Examples

### Basic Memory Operations
```bash
initialize                                          # Initialize system
process-smi                                         # View memory overview
vmstat                                              # View detailed statistics
screen -s worker 256                                # Create process with 256 bytes
screen -c calculator 512 "DECLARE x 10; DECLARE y 5; ADD x x y; PRINT(\"Sum: \" + x)"
```

### Memory Testing
```bash
screen -c memtest 256 "DECLARE val 42; WRITE 0x100 val; READ result 0x100; PRINT(\"Value: \" + result)"
screen -c violation 128 "WRITE 0x200 99"           # Causes memory violation
screen -r violation                                 # Shows violation details
```

## Integration with MO1 Features
- **Backward Compatibility**: All MO1 features preserved and functional
- **Enhanced Process Creation**: Memory-aware process generation
- **Improved Monitoring**: Memory information added to existing displays
- **Extended Error Handling**: Memory violations integrated with process lifecycle

## Files Modified/Created
- Modified: 8 existing files enhanced with memory features
- No new files created (enhanced existing architecture)
- Configuration updated with new memory parameters
- Test documentation and implementation summary added

This implementation successfully extends the CSOPESY simulator with comprehensive memory management while maintaining full compatibility with existing MO1 functionality.
  - Integration with existing process execution

### ✅ Requirement 7: Memory Access Violation Error Messages
- **Status**: COMPLETE
- **Command Enhanced**: `screen -r <process_name>`
- **Features**:
  - Memory access violation detection and logging
  - Timestamp recording (HH:MM:SS format)
  - Invalid memory address tracking
  - Enhanced error message display format

## System Architecture

### Memory Management System
- **Virtual Memory**: Demand paging with page tables
- **Page Replacement**: FIFO algorithm
- **Backing Store**: File-based storage for paged-out memory
- **Memory Validation**: Range checking and access control

### Process Management
- **Memory Allocation**: Per-process memory requirements
- **Instruction Execution**: Support for custom and generated instructions
- **Error Handling**: Comprehensive memory access violation tracking
- **State Management**: Enhanced process states with violation tracking

### Command Interface
- **Memory Commands**: vmstat, process-smi, backing-store
- **Process Commands**: screen -s, screen -c, screen -r, screen -ls
- **System Commands**: scheduler-start/stop, report-util, initialize

## Configuration Parameters
Added to config.txt:
```
max-overall-mem 65536
mem-per-frame 256  
min-mem-per-proc 64
max-mem-per-proc 65536
```

## Memory Access Instructions
- **READ**: `READ variable, 0xAddress` - Load value from memory
- **WRITE**: `WRITE 0xAddress, value` - Store value to memory
- **Validation**: Address range 0x1000 to 0x1000+ProcessMemorySize
- **Error Handling**: Automatic process termination on violations

## Testing Coverage
- Memory allocation validation
- Instruction parsing and execution
- Memory access violation detection
- Process lifecycle with custom instructions
- Command interface integration
- Error message formatting

## Implementation Notes
- All features maintain backward compatibility with MO1
- Thread-safe implementation for concurrent access
- Comprehensive error handling and validation
- Extensible architecture for future enhancements
