#!/bin/bash

# CSOPESY Memory Usage Observation Script
# This script helps demonstrate demand paging memory behavior

echo "🧪 CSOPESY Demand Paging Memory Test"
echo "======================================"
echo ""

echo "📋 Test Instructions:"
echo "1. Run './src/main.exe' in another terminal"
echo "2. Execute the commands shown below"
echo "3. Observe how memory usage changes over time"
echo ""

echo "🚀 Phase 1: Initialize System"
echo "→ initialize"
echo ""

echo "📊 Phase 2: Check Initial Memory State"
echo "→ vmstat"
echo "Expected: Used memory: 0 bytes"
echo ""

echo "🏭 Phase 3: Create Processes (Virtual Allocation)"
echo "→ scheduler-start"
echo ""

echo "📊 Phase 4: Check Memory Immediately After Process Creation"
echo "→ vmstat"
echo "Expected: Used memory: 0 bytes (NO physical frames allocated yet!)"
echo "This is CORRECT behavior for demand paging!"
echo ""

echo "⏱️  Phase 5: Wait for Process Execution"
echo "Wait 3-5 seconds for processes to start executing..."
echo ""

echo "📊 Phase 6: Check Memory After Execution Begins"
echo "→ vmstat"
echo "Expected: Used memory: > 0 bytes (physical frames NOW allocated)"
echo ""

echo "🔍 Phase 7: Monitor Process Activity"
echo "→ screen -ls"
echo "→ process-smi"
echo ""

echo "📈 Phase 8: Continuous Monitoring"
echo "→ Run 'vmstat' every few seconds to see memory changes"
echo ""

echo "🎯 Key Points to Observe:"
echo "- Memory usage = 0 immediately after scheduler-start (CORRECT!)"
echo "- Memory usage > 0 after processes execute for a few seconds"
echo "- 'Num paged in' counter increases as page faults occur"
echo "- Memory usage decreases as processes complete"
echo ""

echo "🔬 Advanced Test - Manual Memory Access:"
echo "→ screen -c memory_test 256 \"WRITE 0x100 42; READ value 0x100\""
echo "→ vmstat"
echo "Expected: Immediate memory usage increase (frame allocated for WRITE)"
echo ""

echo "✅ This demonstrates authentic demand paging behavior!"
echo "   Physical memory is allocated ONLY when actually accessed."
echo ""
