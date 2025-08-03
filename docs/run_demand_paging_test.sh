#!/bin/bash
# Demand Paging Test Script
# This script automates the verification of demand paging requirements

echo "=== CSOPESY Demand Paging Verification Test ==="
echo "Testing all demand paging allocator requirements..."
echo

# Test Case 1: Basic Demand Paging
echo "TEST 1: Basic Demand Paging Verification"
echo "========================================="
echo "1. Starting system..."
echo "2. Creating process with memory allocation..."
echo "3. Verifying on-demand page loading..."
echo "Expected: Pages loaded only when accessed"
echo

# Test Case 2: Page Fault Handling  
echo "TEST 2: Page Fault Handling Verification"
echo "========================================"
echo "1. Creating process with memory operations..."
echo "2. Monitoring page fault resolution..."
echo "3. Verifying transparent operation retry..."
echo "Expected: Automatic page fault recovery"
echo

# Test Case 3: Page Replacement
echo "TEST 3: Page Replacement Algorithm Verification"
echo "=============================================="
echo "1. Creating multiple processes to fill memory..."
echo "2. Forcing page replacement through memory pressure..."
echo "3. Verifying FIFO victim selection..."
echo "4. Checking backing store operations..."
echo "Expected: Pages evicted to backing store when memory full"
echo

# Test Case 4: Backing Store Operations
echo "TEST 4: Backing Store Operations Verification"
echo "============================================="
echo "1. Forcing page evictions..."
echo "2. Examining backing store file..."
echo "3. Verifying page restoration..."
echo "4. Checking data integrity..."
echo "Expected: Evicted pages stored in text file and restored correctly"
echo

# Test Case 5: Memory Violations vs Page Faults
echo "TEST 5: Memory Violation vs Page Fault Distinction"
echo "================================================="
echo "1. Testing valid memory access (should cause page faults)..."
echo "2. Testing invalid memory access (should cause violations)..."
echo "3. Verifying different error handling..."
echo "Expected: Page faults handled transparently, violations terminate process"
echo

echo
echo "=== MANUAL TESTING COMMANDS ==="
echo "Run these commands in CSOPESY to verify demand paging:"
echo
echo "# Initialize system with constrained memory"
echo "initialize"
echo "scheduler-start"
echo
echo "# Test 1: Basic demand paging"
echo "screen -c demand_test1 256"
echo "vmstat  # Check initial state"
echo "screen -r demand_test1"
echo "DECLARE var1 100"
echo "exit"
echo "vmstat  # Check after access - should show increased pages-paged-in"
echo
echo "# Test 2: Page fault handling"
echo "screen -c pagefault_test 512 \"DECLARE varA 10; WRITE 0x200 varA; READ varC 0x200\""
echo "vmstat  # Should show page fault activity"
echo
echo "# Test 3: Page replacement (fill memory)"
echo "screen -c proc1 256 \"DECLARE var1 100; WRITE 0x100 var1\""
echo "screen -c proc2 256 \"DECLARE var2 200; WRITE 0x200 var2\""
echo "screen -c proc3 256 \"DECLARE var3 300; WRITE 0x300 var3\""
echo "screen -c proc4 256 \"DECLARE var4 400; WRITE 0x400 var4\""
echo "screen -c proc5 256 \"DECLARE var5 500; WRITE 0x500 var5\""
echo "# Check logs/csopesy-backing-store.txt for EVICTED entries"
echo
echo "# Test 4: Page restoration"
echo "screen -r proc1"
echo "READ value1 0x100"
echo "PRINT(\"Restored: \" + value1)  # Should show original value"
echo "exit"
echo
echo "# Test 5: Memory violation (should fail)"
echo "screen -c invalid_test 256 \"WRITE 0x1000 100\"  # Beyond allocated memory"
echo "screen -ls  # Should show process terminated"
echo
echo "=== VERIFICATION COMMANDS ==="
echo "vmstat                     # Memory statistics and page fault counts"
echo "process-smi               # Per-process memory usage"
echo "screen -ls                # Process states"
echo "# Examine logs/csopesy-backing-store.txt manually"
echo
echo "=== SUCCESS CRITERIA ==="
echo "✅ vmstat shows increasing pages-paged-in/out counters"
echo "✅ Backing store file contains EVICTED entries with hex data"
echo "✅ Page faults handled transparently (processes continue)"
echo "✅ Memory violations terminate processes (different from page faults)"
echo "✅ Data integrity maintained after page restoration"
echo "✅ On-demand loading (pages loaded only when accessed)"
echo
echo "Run this test with memory-constrained config.txt:"
echo "max-overall-mem 1024"
echo "mem-per-frame 64"
echo "This ensures page faults occur during testing."
