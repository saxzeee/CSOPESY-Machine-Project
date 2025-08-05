@echo off
echo Building CSOPESY OS Emulator...
cd src
g++ -std=c++17 -g main.cpp commands/command_processor.cpp config/config.cpp memory/memory_manager.cpp process/process.cpp scheduler/scheduler.cpp utils/utils.cpp -o main.exe
if exist main.exe (
    echo Build successful! Executable created at main.exe
) else (
    echo Build failed!
    exit /b 1
)
