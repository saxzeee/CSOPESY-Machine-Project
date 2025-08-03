# Testing Memory Access Violation Tracking (Requirement 7)

## Test Case: Memory Access Violation Detection

### Steps to Test:

1. Initialize the system:
   ```
   initialize
   ```

2. Start the scheduler:
   ```
   scheduler-start
   ```

3. Create a process with custom instructions that will cause a memory access violation:
   ```
   screen -c test_violation 256 "READ var1, 0x2000"
   ```
   
   Note: 0x2000 is outside the valid memory range for a 256-byte process (valid range is 0x1000 to 0x10FF)

4. Wait a moment for the process to execute and encounter the violation

5. Try to resume the process that should have terminated due to memory access violation:
   ```
   screen -r test_violation
   ```

### Expected Result:
The command should display:
```
Process test_violation shut down due to memory access violation error that occurred at HH:MM:SS. 0x2000 invalid.
```

### Alternative Test with Invalid Address Format:
```
screen -c test_violation2 512 "WRITE 0xGGGG, 100"
```

This should also trigger a memory access violation due to invalid address format.

### Test with Valid Process (Control):
```
screen -c test_valid 256 "READ var1, 0x1000; WRITE 0x1010, 42"
screen -r test_valid
```

This should work normally and show the process screen interface.
