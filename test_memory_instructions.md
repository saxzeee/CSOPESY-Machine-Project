# Testing Memory Access Instructions (Requirement 5)

This document outlines test cases for Requirement 5: Simulating memory access via process instruction.

## New Instructions Implemented

### READ Instruction
- **Syntax**: `READ(var, memory_address)`
- **Function**: Reads a uint16 value from memory address and stores it in variable
- **Returns 0** if memory address is uninitialized

### WRITE Instruction  
- **Syntax**: `WRITE(memory_address, value)`
- **Function**: Writes uint16 value to the specified memory address

## Test Cases

### 1. Valid Memory Operations
```
READ my_var 0x1000          // Read from address 0x1000 (returns 0 if uninitialized)
WRITE 0x2000 42             // Write value 42 to address 0x2000
READ my_var_2 0x2000        // Read from address 0x2000 (should return 42)
WRITE 0x1500 65535          // Write maximum uint16 value
READ max_val 0x1500         // Should return 65535
```

### 2. Variable Limit Testing
- Symbol table size: 64 bytes
- Maximum variables: 32 (each uint16 = 2 bytes)
- Variables beyond 32nd declaration should be ignored

### 3. Memory Access Violations (Should terminate process)
```
READ invalid_var 0x500      // Address below process memory space
WRITE 0xFFFF 100            // Address above process memory space  
READ bad_format ABC123      // Invalid hexadecimal format
WRITE invalid_addr 50       // Invalid address format
```

### 4. Value Clamping
- uint16 values are clamped between 0 and 65535
- Negative values become 0
- Values > 65535 become 65535

## Implementation Details

### Memory Address Validation
- Valid range: 0x1000 to (0x1000 + memoryRequired)
- Addresses outside this range trigger ACCESS VIOLATION
- Process terminates immediately on access violation

### Symbol Table Management
- Fixed size: 64 bytes
- Maximum 32 variables (32 * 2 bytes = 64 bytes)
- Exceeding limit ignores subsequent DECLARE/READ instructions

### Memory Representation
- Emulated memory space (not 1:1 physical mapping)
- Each process has isolated memory space
- Memory addresses are generated in even increments (aligned to 2-byte boundaries)

### Instruction Generation
- READ and WRITE instructions now included in automatic process generation
- Memory addresses generated in valid ranges
- Proper hexadecimal formatting for addresses

## Expected Behavior

1. **Valid Operations**: Store/retrieve values correctly, show proper logging
2. **Access Violations**: Display "ACCESS VIOLATION" message and terminate process
3. **Variable Limit**: Show "Variable limit reached" message for excess declarations
4. **Uninitialized Read**: Return 0 for uninitialized memory addresses
5. **Value Clamping**: Automatically clamp values to uint16 range

## Testing Commands

1. Create process with memory: `screen -s test_proc 1024`
2. Start scheduler: `scheduler-start` (will generate READ/WRITE instructions)
3. Monitor process: `process-smi` and `vmstat`
4. Manual testing within process screen sessions

## Automatic Generation

When using `scheduler-start`, processes will now automatically generate:
- READ instructions with random addresses in valid range
- WRITE instructions with random values and addresses
- Proper error handling for access violations
- Variable limit enforcement
