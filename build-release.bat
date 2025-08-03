@echo off
echo Building CSOPESY OS Emulator (Release)...
cd src
g++ -O2 -DNDEBUG -std=c++17 main.cpp core/config.cpp utils/utils.cpp process/process.cpp scheduler/scheduler.cpp commands/command_processor.cpp memory/memory_manager.cpp -o main.exe
if exist main.exe (
    echo Release build successful! Executable created at main.exe
) else (
    echo Release build failed!
    exit /b 1
)
