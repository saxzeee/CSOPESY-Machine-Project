# Demand Paging Test Script for Windows PowerShell
# This script provides test commands for verifying demand paging requirements

Write-Host "=== CSOPESY Demand Paging Verification Test ===" -ForegroundColor Green
Write-Host "Testing all demand paging allocator requirements..." -ForegroundColor Yellow
Write-Host ""

Write-Host "PREREQUISITES:" -ForegroundColor Red
Write-Host "1. Ensure config.txt has memory-constrained settings:" -ForegroundColor White
Write-Host "   max-overall-mem 1024" -ForegroundColor Cyan
Write-Host "   mem-per-frame 64" -ForegroundColor Cyan
Write-Host "   This forces page faults for testing" -ForegroundColor White
Write-Host ""

Write-Host "2. Build and run CSOPESY:" -ForegroundColor White
Write-Host "   g++ -g src/main.cpp -o src/main.exe" -ForegroundColor Cyan
Write-Host "   ./src/main.exe" -ForegroundColor Cyan
Write-Host ""

Write-Host "TEST SEQUENCE:" -ForegroundColor Green
Write-Host "=============" -ForegroundColor Green
Write-Host ""

Write-Host "TEST 1: Basic Demand Paging Verification" -ForegroundColor Yellow
Write-Host "=========================================" -ForegroundColor Yellow
Write-Host "Commands to run in CSOPESY:" -ForegroundColor White
Write-Host "initialize" -ForegroundColor Cyan
Write-Host "scheduler-start" -ForegroundColor Cyan
Write-Host "screen -c demand_test1 256" -ForegroundColor Cyan
Write-Host "vmstat                           # Check initial state - low pages-paged-in" -ForegroundColor Cyan
Write-Host "screen -r demand_test1" -ForegroundColor Cyan
Write-Host "DECLARE var1 100                # Trigger page loading" -ForegroundColor Cyan
Write-Host "exit" -ForegroundColor Cyan
Write-Host "vmstat                           # Should show increased pages-paged-in" -ForegroundColor Cyan
Write-Host ""

Write-Host "TEST 2: Page Fault Handling" -ForegroundColor Yellow
Write-Host "============================" -ForegroundColor Yellow
Write-Host "screen -c pagefault_test 512 `"DECLARE varA 10; WRITE 0x200 varA; READ varC 0x200`"" -ForegroundColor Cyan
Write-Host "vmstat                           # Should show page fault activity" -ForegroundColor Cyan
Write-Host "process-smi                      # Check process memory usage" -ForegroundColor Cyan
Write-Host ""

Write-Host "TEST 3: Page Replacement Algorithm" -ForegroundColor Yellow
Write-Host "===================================" -ForegroundColor Yellow
Write-Host "# Fill physical memory to force page replacement:" -ForegroundColor White
Write-Host "screen -c proc1 256 `"DECLARE var1 100; WRITE 0x100 var1`"" -ForegroundColor Cyan
Write-Host "screen -c proc2 256 `"DECLARE var2 200; WRITE 0x200 var2`"" -ForegroundColor Cyan
Write-Host "screen -c proc3 256 `"DECLARE var3 300; WRITE 0x300 var3`"" -ForegroundColor Cyan
Write-Host "screen -c proc4 256 `"DECLARE var4 400; WRITE 0x400 var4`"" -ForegroundColor Cyan
Write-Host "screen -c proc5 256 `"DECLARE var5 500; WRITE 0x500 var5`"  # Should trigger eviction" -ForegroundColor Cyan
Write-Host ""
Write-Host "# Check backing store file:" -ForegroundColor White
Write-Host "Get-Content logs/csopesy-backing-store.txt    # Should show EVICTED entries" -ForegroundColor Cyan
Write-Host ""

Write-Host "TEST 4: Backing Store Operations" -ForegroundColor Yellow
Write-Host "=================================" -ForegroundColor Yellow
Write-Host "# Verify page restoration:" -ForegroundColor White
Write-Host "screen -r proc1" -ForegroundColor Cyan
Write-Host "READ value1 0x100" -ForegroundColor Cyan
Write-Host "PRINT(`"Restored: `" + value1)      # Should show original value (100)" -ForegroundColor Cyan
Write-Host "exit" -ForegroundColor Cyan
Write-Host "vmstat                           # Should show both paged-in and paged-out counts" -ForegroundColor Cyan
Write-Host ""

Write-Host "TEST 5: Memory Violation vs Page Fault" -ForegroundColor Yellow
Write-Host "=======================================" -ForegroundColor Yellow
Write-Host "# Valid access (page fault - should succeed):" -ForegroundColor White
Write-Host "screen -c valid_access 256 `"DECLARE var 100; WRITE 0x80 var`"" -ForegroundColor Cyan
Write-Host ""
Write-Host "# Invalid access (memory violation - should fail):" -ForegroundColor White
Write-Host "screen -c invalid_access 256 `"WRITE 0x1000 100`"" -ForegroundColor Cyan
Write-Host "screen -ls                       # invalid_access should be TERMINATED" -ForegroundColor Cyan
Write-Host ""

Write-Host "VERIFICATION COMMANDS:" -ForegroundColor Green
Write-Host "=====================" -ForegroundColor Green
Write-Host "vmstat                           # Memory statistics and page fault counts" -ForegroundColor Cyan
Write-Host "process-smi                      # Per-process memory usage" -ForegroundColor Cyan
Write-Host "screen -ls                       # Process states" -ForegroundColor Cyan
Write-Host "Get-Content logs/csopesy-backing-store.txt    # Examine backing store file" -ForegroundColor Cyan
Write-Host ""

Write-Host "SUCCESS CRITERIA:" -ForegroundColor Green
Write-Host "=================" -ForegroundColor Green
Write-Host "✅ vmstat shows increasing pages-paged-in when pages accessed" -ForegroundColor White
Write-Host "✅ vmstat shows pages-paged-out when memory pressure occurs" -ForegroundColor White
Write-Host "✅ Backing store file contains EVICTED entries with:" -ForegroundColor White
Write-Host "   - Process metadata (Process=X Page=Y Frame=Z)" -ForegroundColor Gray
Write-Host "   - Hexadecimal page data in 16-byte rows" -ForegroundColor Gray
Write-Host "✅ Page faults handled transparently (processes continue execution)" -ForegroundColor White
Write-Host "✅ Memory violations terminate processes (different behavior)" -ForegroundColor White
Write-Host "✅ Data integrity maintained (restored values match original)" -ForegroundColor White
Write-Host "✅ On-demand loading (pages loaded only when accessed, not at allocation)" -ForegroundColor White
Write-Host ""

Write-Host "EXPECTED BACKING STORE FORMAT:" -ForegroundColor Green
Write-Host "==============================" -ForegroundColor Green
Write-Host "EVICTED: Process=proc1 Page=0 Frame=0" -ForegroundColor Gray
Write-Host "64 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" -ForegroundColor Gray
Write-Host "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" -ForegroundColor Gray
Write-Host "..." -ForegroundColor Gray
Write-Host ""

Write-Host "Ready to test! Run CSOPESY and execute the commands above." -ForegroundColor Green
