@echo off
echo Building CSOPESY OS Emulator...
cd src
g++ -std=c++17 main.cpp core/config.cpp utils/utils.cpp process/process.cpp scheduler/scheduler.cpp commands/command_processor.cpp cpu/cpu.cpp memory/memory_manager.cpp -o ../build/csopesy.exe
cd ..
if exist build\csopesy.exe (
    echo Build successful! Executable created at build\csopesy.exe
) else (
    echo Build failed!
    exit /b 1
)
