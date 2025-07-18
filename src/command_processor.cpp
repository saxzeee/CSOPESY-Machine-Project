#include "emulator.h"

CommandProcessor::CommandProcessor() {
    initializeCommands();
}

void CommandProcessor::initializeCommands() {
    commands["initialize"] = [this](const std::vector<std::string>& args) { handleInitialize(args); };
    commands["screen"] = [this](const std::vector<std::string>& args) {
        if (args.size() >= 2) {
            if (args[1] == "-s" && args.size() >= 3) {
                handleScreenS({args[2]});
            } else if (args[1] == "-r" && args.size() >= 3) {
                handleScreenR({args[2]});
            } else if (args[1] == "-ls") {
                handleScreenLS({});
            }
        }
    };
    commands["scheduler-start"] = [this](const std::vector<std::string>& args) { handleSchedulerStart(args); };
    commands["scheduler-stop"] = [this](const std::vector<std::string>& args) { handleSchedulerStop(args); };
    commands["report-util"] = [this](const std::vector<std::string>& args) { handleReportUtil(args); };
    commands["process-smi"] = [this](const std::vector<std::string>& args) { handleProcessSMI(args); };
    commands["help"] = [this](const std::vector<std::string>& args) { handleHelp(args); };
    commands["clear"] = [this](const std::vector<std::string>& args) { handleClear(args); };
}

std::vector<std::string> CommandProcessor::parseCommand(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

void CommandProcessor::run() {
    std::string input;
    
    while (true) {
        Utils::setTextColor(36); 
        std::cout << "\nEnter a command: ";
        Utils::resetTextColor();
        
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        
        if (lowerInput == "exit") {
            if (scheduler && scheduler->isSystemRunning()) {
                std::cout << "Stopping scheduler before exit..." << std::endl;
                scheduler->stop();
            }
            std::cout << "Goodbye!" << std::endl;
            break;
        }
        
        auto tokens = parseCommand(lowerInput);
        if (tokens.empty()) continue;
        
        std::string command = tokens[0];
        
        if (tokens.size() >= 2) {
            if (command == "screen") {
                command = "screen";
            } else if (command == "scheduler-start" || 
                      (command == "scheduler" && tokens.size() > 1 && tokens[1] == "start")) {
                command = "scheduler-start";
            } else if (command == "scheduler-stop" || 
                      (command == "scheduler" && tokens.size() > 1 && tokens[1] == "stop")) {
                command = "scheduler-stop";
            } else if (command == "report-util") {
                command = "report-util";
            } else if (command == "process-smi") {
                command = "process-smi";
            }
        }
        
        auto it = commands.find(command);
        if (it != commands.end()) {
            try {
                it->second(tokens);
            } catch (const std::exception& e) {
                Utils::setTextColor(31); 
                std::cerr << "Error executing command: " << e.what() << std::endl;
                Utils::resetTextColor();
            }
        } else {
            Utils::setTextColor(33); 
            std::cout << "Command not recognized. Type 'help' for available commands." << std::endl;
            Utils::resetTextColor();
        }
    }
}

void CommandProcessor::displayHeader() {
    Utils::clearScreen();
    Utils::setTextColor(34); 
    
    std::cout << R"(
     _/_/_/    _/_/_/    _/_/    _/_/_/    _/_/_/_/    _/_/_/  _/      _/  
  _/        _/        _/    _/  _/    _/  _/        _/          _/  _/     
 _/          _/_/    _/    _/  _/_/_/    _/_/_/      _/_/        _/        
_/              _/  _/    _/  _/        _/              _/      _/         
 _/_/_/  _/_/_/      _/_/    _/        _/_/_/_/  _/_/_/        _/          
)" << std::endl;
    
    Utils::setTextColor(32);
    std::cout << "Welcome to CSOPESY Enhanced OS Emulator!" << std::endl;
    std::cout << "Type 'help' to see available commands or 'exit' to quit." << std::endl;
    Utils::resetTextColor();
    std::cout << std::string(70, '=') << std::endl;
}

void CommandProcessor::handleInitialize(const std::vector<std::string>& args) {
    std::string configFile = "config.txt";
    if (args.size() > 1) {
        configFile = args[1];
    }
    
    auto config = std::make_unique<SystemConfig>();
    if (config->loadFromFile(configFile)) {
        config->display();
        scheduler = std::make_unique<Scheduler>(std::move(config));
        initialized = true;
        
        Utils::setTextColor(32); 
        std::cout << "System initialized successfully!" << std::endl;
        Utils::resetTextColor();
    } else {
        Utils::setTextColor(31);
        std::cout << "Failed to initialize system. Check config file: " << configFile << std::endl;
        Utils::resetTextColor();
    }
}

void CommandProcessor::handleScreenS(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    if (args.empty()) {
        std::cout << "Usage: screen -s <process_name>" << std::endl;
        return;
    }
    
    std::string processName = args[0];
    
    auto process = scheduler->findProcess(processName);
    if (!process) {
        if (scheduler->createProcess(processName)) {
            process = scheduler->findProcess(processName);
        } else {
            return; 
        }
    }
    
    Utils::clearScreen();
    std::cout << "=== Screen Session: " << processName << " ===" << std::endl;
    std::cout << "Process ID: " << process->pid << std::endl;
    std::cout << "State: " << process->getStateString() << std::endl;
    std::cout << "Progress: " << process->executedInstructions << "/" << process->totalInstructions << std::endl;
    std::cout << "Created: " << process->creationTimestamp << std::endl;
    
    if (!process->instructionHistory.empty()) {
        std::cout << "\nRecent Instructions:" << std::endl;
        int start = std::max(0, static_cast<int>(process->instructionHistory.size()) - 10);
        for (int i = start; i < process->instructionHistory.size(); ++i) {
            std::cout << process->instructionHistory[i] << std::endl;
        }
    }
    
    std::cout << "\nCommands: 'process-smi' for details, 'exit' to return" << std::endl;
    
    std::string input;
    while (true) {
        std::cout << process->name << "$ ";
        std::getline(std::cin, input);
        
        if (input == "exit") {
            displayHeader();
            break;
        } else if (input == "process-smi") {
            handleProcessSMI({process->name});
        } else {
            std::cout << "Available commands: process-smi, exit" << std::endl;
        }
    }
}

void CommandProcessor::handleScreenR(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    if (args.empty()) {
        std::cout << "Usage: screen -r <process_name>" << std::endl;
        return;
    }
    
    std::string processName = args[0];
    auto process = scheduler->findProcess(processName);
    
    if (!process) {
        std::cout << "Process '" << processName << "' not found." << std::endl;
        return;
    }
    
    handleScreenS({processName}); 
}

void CommandProcessor::handleScreenLS(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    scheduler->displaySystemStatus();
    scheduler->displayProcesses();
}

void CommandProcessor::handleSchedulerStart(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    if (scheduler->start()) {
        Utils::setTextColor(32); 
        std::cout << "Scheduler started successfully!" << std::endl;
        Utils::resetTextColor();
    } else {
        Utils::setTextColor(33); 
        std::cout << "Scheduler is already running." << std::endl;
        Utils::resetTextColor();
    }
}

void CommandProcessor::handleSchedulerStop(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    scheduler->stop();
    Utils::setTextColor(32); 
    std::cout << "Scheduler stopped successfully!" << std::endl;
    Utils::resetTextColor();
}

void CommandProcessor::handleReportUtil(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    std::string filename = "csopesy-log.txt";
    if (args.size() > 1) {
        filename = args[1];
    }
    
    scheduler->generateReport(filename);
}

void CommandProcessor::handleProcessSMI(const std::vector<std::string>& args) {
    if (!initialized) {
        std::cout << "Please initialize the system first." << std::endl;
        return;
    }
    
    if (args.empty()) {
        scheduler->displaySystemStatus();
        scheduler->displayProcesses();
    } else {
        std::string processName = args[0];
        auto process = scheduler->findProcess(processName);
        
        if (!process) {
            std::cout << "Process '" << processName << "' not found." << std::endl;
            return;
        }
        
        std::cout << "\n=== Process Information ===" << std::endl;
        std::cout << "Process ID: " << process->pid << std::endl;
        std::cout << "Name: " << process->name << std::endl;
        std::cout << "State: " << process->getStateString() << std::endl;
        std::cout << "Core Assignment: " << (process->coreAssignment >= 0 ? std::to_string(process->coreAssignment) : "None") << std::endl;
        std::cout << "Memory Size: " << process->memorySize << " KB" << std::endl;
        std::cout << "Instructions: " << process->executedInstructions << "/" << process->totalInstructions << std::endl;
        std::cout << "Created: " << process->creationTimestamp << std::endl;
        
        if (process->state == ProcessState::TERMINATED) {
            std::cout << "Completed: " << process->completionTimestamp << std::endl;
            std::cout << "Turnaround Time: " << process->turnaroundTime << "ms" << std::endl;
            std::cout << "Waiting Time: " << process->waitingTime << "ms" << std::endl;
        }
        
        std::cout << "=========================" << std::endl;
    }
}

void CommandProcessor::handleHelp(const std::vector<std::string>& args) {
    Utils::setTextColor(36); 
    std::cout << "\n=== CSOPESY Enhanced Commands ===" << std::endl;
    Utils::resetTextColor();
    
    std::cout << std::left;
    std::cout << std::setw(25) << "initialize" << "Initialize the system with config.txt" << std::endl;
    std::cout << std::setw(25) << "screen -s <name>" << "Create/attach to a process screen session" << std::endl;
    std::cout << std::setw(25) << "screen -r <name>" << "Resume an existing process screen session" << std::endl;
    std::cout << std::setw(25) << "screen -ls" << "List all processes and system status" << std::endl;
    std::cout << std::setw(25) << "scheduler-start" << "Start the process scheduler" << std::endl;
    std::cout << std::setw(25) << "scheduler-stop" << "Stop the process scheduler" << std::endl;
    std::cout << std::setw(25) << "report-util" << "Generate system report to file" << std::endl;
    std::cout << std::setw(25) << "process-smi" << "Show detailed process information" << std::endl;
    std::cout << std::setw(25) << "clear" << "Clear the screen" << std::endl;
    std::cout << std::setw(25) << "help" << "Show this help message" << std::endl;
    std::cout << std::setw(25) << "exit" << "Exit the emulator" << std::endl;
    
    Utils::setTextColor(33); 
    std::cout << "\nScreen Session Commands:" << std::endl;
    Utils::resetTextColor();
    std::cout << std::setw(25) << "process-smi" << "Show process details within screen" << std::endl;
    std::cout << std::setw(25) << "exit" << "Exit the screen session" << std::endl;
    
    std::cout << std::string(60, '=') << std::endl;
}

void CommandProcessor::handleClear(const std::vector<std::string>& args) {
    displayHeader();
}
