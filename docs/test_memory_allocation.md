# Testing Memory Allocation Requirements

This document outlines test cases for Requirement 4: Required memory per process.

## Test Cases

### 1. Valid Memory Allocations (Powers of 2 within range)
- `screen -s process1 64` ✓ (2^6)
- `screen -s process2 128` ✓ (2^7)
- `screen -s process3 256` ✓ (2^8)
- `screen -s process4 512` ✓ (2^9)
- `screen -s process5 1024` ✓ (2^10)
- `screen -s process6 2048` ✓ (2^11)
- `screen -s process7 4096` ✓ (2^12)
- `screen -s process8 8192` ✓ (2^13)
- `screen -s process9 16384` ✓ (2^14)
- `screen -s process10 32768` ✓ (2^15)
- `screen -s process11 65536` ✓ (2^16)

### 2. Invalid Memory Allocations (Should show "invalid memory allocation" message)

#### Below minimum (< 64 bytes)
- `screen -s test1 32` ✗ (Below minimum)
- `screen -s test2 16` ✗ (Below minimum)
- `screen -s test3 8` ✗ (Below minimum)

#### Above maximum (> 65536 bytes)
- `screen -s test4 131072` ✗ (Above maximum)
- `screen -s test5 262144` ✗ (Above maximum)

#### Not powers of 2 (within range but invalid)
- `screen -s test6 100` ✗ (Not power of 2)
- `screen -s test7 200` ✗ (Not power of 2)
- `screen -s test8 300` ✗ (Not power of 2)
- `screen -s test9 1000` ✗ (Not power of 2)
- `screen -s test10 65000` ✗ (Not power of 2)

#### Invalid format
- `screen -s test11 abc` ✗ (Invalid number format)
- `screen -s test12` ✗ (Missing memory size parameter)

### 3. Command Usage
- `screen -s` (without parameters) should show usage instructions
- `help` command should show updated syntax for screen -s

## Expected Behavior

1. **Valid allocations**: Process should be created successfully with the specified memory allocation
2. **Invalid allocations**: Should display "Invalid memory allocation. Memory size must be between 64 and 65536 bytes and a power of 2."
3. **Missing parameters**: Should show usage instructions
4. **Help command**: Should show updated syntax: `screen -s <name> <mem>`

## Implementation Details

- Memory validation functions added to Utils namespace:
  - `isPowerOfTwo(size_t n)`: Checks if a number is a power of 2
  - `isValidMemorySize(size_t memorySize)`: Validates memory size is within range [64, 65536] and is a power of 2
- New overloaded `createProcess(const std::string& name, size_t customMemorySize)` method
- Updated screen -s command to require memory size parameter
- Updated help text and usage instructions
