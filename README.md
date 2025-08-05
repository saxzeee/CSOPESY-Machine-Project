# CSOPESY-Machine-Project

A modular operating system emulator implemented in C++.

## Project Structure

```
CSOPESY-Machine-Project/
├── README.md
├── config.txt                       # System configuration
├── logs/                            # Runtime logs and data
│   ├── csopesy-log.txt
│   └── csopesy-backing-store.txt
├── src/                            # Source code
│   ├── main.cpp                    # Entry point
│   ├── config/                     # Configuration management
│   │   ├── config.h
│   │   └── config.cpp
│   ├── process/                    # Process management
│   │   ├── process.h
│   │   └── process.cpp
│   ├── scheduler/                  # Scheduling algorithms
│   │   ├── scheduler.h
│   │   └── scheduler.cpp
│   ├── cpu/                        # CPU simulation (placeholder)
│   │   ├── cpu.h
│   │   └── cpu.cpp
│   ├── memory/                     # Memory management (placeholder)
│   │   ├── memory_manager.h
│   │   └── memory_manager.cpp
│   ├── commands/                   # Command processing
│   │   ├── command_processor.h
│   │   └── command_processor.cpp
│   └── utils/                      # Utility functions
│       ├── utils.h
│       └── utils.cpp
└── build/                          # Compiled executables
    └── csopesy.exe
```

## Building

From the `src` directory:
```bash
g++ main.cpp core\config.cpp utils\utils.cpp process\process.cpp scheduler\scheduler.cpp commands\command_processor.cpp cpu\cpu.cpp memory\memory_manager.cpp -o ..\build\csopesy.exe
```

## Running

From the project root directory:
```bash
.\build\csopesy.exe
```

## Features

- Modular architecture with clear separation of concerns
- Process simulation with instruction execution
- FCFS and Round Robin scheduling algorithms
- Command-line interface with screen sessions
- System monitoring and reporting
- Configurable system parameters

## Commands

- `initialize` - Initialize system with config.txt
- `screen -ls` - List all processes and system status
- `screen -s <name>` - Create/attach to process screen session
- `screen -r <name>` - Resume existing process screen session
- `scheduler-start` - Enable automatic process generation
- `scheduler-stop` - Disable automatic process generation
- `report-util` - Generate system report
- `help` - Show all commands
- `exit` - Exit the emulator