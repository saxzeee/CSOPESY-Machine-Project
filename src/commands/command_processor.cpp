#include "command_processor.h"
#include "../utils/utils.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

CommandProcessor::CommandProcessor() {
    commands["initialize"] = [this](const std::vector<std::string>& args) {
        std::string configFile = "config.txt";
        if (args.size() > 1) {
            configFile = args[1];
        }
        
        auto config = std::make_unique<SystemConfig>();
        if (config->loadFromFile(configFile)) {
            config->display();
            scheduler = std::make_unique<Scheduler>(std::move(config));
            initialized = true;
            
            std::cout << "System initialized successfully!" << std::endl;
        } else {
            std::cout << "Failed to initialize system. Check config file: " << configFile << std::endl;
        }
    };
    
    commands["scheduler-start"] = [this](const std::vector<std::string>& args) {
        if (!initialized) {
            std::cout << "Please initialize the system first." << std::endl;
            return;
        }
        
        if (!scheduler->isSystemRunning()) {
            if (scheduler->start()) {
                Utils::setTextColor(32); 
                std::cout << "Scheduler auto-started for dummy process generation." << std::endl;
                Utils::resetTextColor();
            }
        }
        
        scheduler->enableDummyProcessGeneration();
    };
    
    commands["scheduler-test"] = [this](const std::vector<std::string>& args) {
        if (!initialized) {
            std::cout << "Please initialize the system first." << std::endl;
            return;
        }
        
        if (scheduler->startTestMode()) {
            Utils::setTextColor(32); 
            std::cout << "Scheduler test mode started successfully!" << std::endl;
            Utils::resetTextColor();
        }
    };
    
    commands["scheduler-stop"] = [this](const std::vector<std::string>& args) {
        if (!initialized) {
            std::cout << "Please initialize the system first." << std::endl;
            return;
        }
        
        scheduler->disableDummyProcessGeneration();
        
        Utils::setTextColor(32); 
        std::cout << "Dummy process generation stopped successfully!" << std::endl;
        std::cout << "Existing processes will continue to execute." << std::endl;
        Utils::resetTextColor();
    };
    
    commands["screen"] = [this](const std::vector<std::string>& args) {
        if (!initialized) {
            std::cout << "Please initialize the system first." << std::endl;
            return;
        }
        
        if (args.size() >= 2 && args[1] == "-ls") {
            scheduler->displaySystemStatus();
            scheduler->displayProcesses();
        } else if (args.size() >= 3 && args[1] == "-s") {
            std::string processName = args[2];
            
            auto process = scheduler->findProcess(processName);
            if (!process) {
                scheduler->createProcess(processName);
                process = scheduler->findProcess(processName);
            }
            
            if (process) {
                Utils::clearScreen();
                Utils::setTextColor(36); 
                std::cout << "Process name: " << process->name << std::endl;
                std::cout << "Instruction: Line " << process->executedInstructions << " / " << process->totalInstructions << std::endl;
                std::cout << "Created at: " << process->creationTimestamp << std::endl;
                Utils::resetTextColor();
                
                if (!process->instructionHistory.empty()) {
                    std::cout << "\n--- Process Logs ---" << std::endl;
                    int start = std::max(0, static_cast<int>(process->instructionHistory.size()) - 10);
                    for (int i = start; i < process->instructionHistory.size(); ++i) {
                        std::cout << process->instructionHistory[i] << std::endl;
                    }
                }
                
                std::string input;
                while (true) {
                    std::cout << "\n>> ";
                    std::getline(std::cin, input);
                    
                    if (input == "exit") {
                        displayHeader();
                        break;
                    } else if (input == "process-smi") {
                        std::cout << "\nProcess name: " << process->name << std::endl;
                        std::cout << "ID: " << process->pid << std::endl;
                        std::cout << "Logs:" << std::endl;
                        
                        if (!process->instructionHistory.empty()) {
                            for (const auto& log : process->instructionHistory) {
                                std::cout << log << std::endl;
                            }
                        } else {
                            std::cout << "No logs found for this process." << std::endl;
                        }
                        
                        std::cout << std::endl;
                        if (process->state == ProcessState::TERMINATED) {
                            std::cout << "Finished!" << std::endl;
                        } else {
                            std::cout << "Current instruction line: " << process->executedInstructions << std::endl;
                            std::cout << "Lines of code: " << process->totalInstructions << std::endl;
                        }
                    } else {
                        std::cout << "Available commands: process-smi, exit" << std::endl;
                    }
                }
            }
        } else if (args.size() >= 3 && args[1] == "-r") {
            std::string processName = args[2];
            
            auto process = scheduler->findProcess(processName);
            if (!process) {
                std::cout << "Process " << processName << " not found." << std::endl;
                return;
            }
            
            if (process->state == ProcessState::TERMINATED) {
                std::cout << "Process " << processName << " has already finished." << std::endl;
                return;
            }
            
            Utils::clearScreen();
            Utils::setTextColor(36); 
            std::cout << "Process name: " << process->name << std::endl;
            std::cout << "Instruction: Line " << process->executedInstructions << " / " << process->totalInstructions << std::endl;
            std::cout << "Created at: " << process->creationTimestamp << std::endl;
            Utils::resetTextColor();
            
            if (!process->instructionHistory.empty()) {
                std::cout << "\n--- Process Logs ---" << std::endl;
                int start = std::max(0, static_cast<int>(process->instructionHistory.size()) - 10);
                for (int i = start; i < process->instructionHistory.size(); ++i) {
                    std::cout << process->instructionHistory[i] << std::endl;
                }
            }
            
            std::string input;
            while (true) {
                std::cout << "\n>> ";
                std::getline(std::cin, input);
                
                if (input == "exit") {
                    displayHeader();
                    break;
                } else if (input == "process-smi") {
                    std::cout << "\nProcess name: " << process->name << std::endl;
                    std::cout << "ID: " << process->pid << std::endl;
                    std::cout << "Logs:" << std::endl;
                    
                    if (!process->instructionHistory.empty()) {
                        for (const auto& log : process->instructionHistory) {
                            std::cout << log << std::endl;
                        }
                    } else {
                        std::cout << "No logs found for this process." << std::endl;
                    }
                    
                    std::cout << std::endl;
                    if (process->state == ProcessState::TERMINATED) {
                        std::cout << "Finished!" << std::endl;
                    } else {
                        std::cout << "Current instruction line: " << process->executedInstructions << std::endl;
                        std::cout << "Lines of code: " << process->totalInstructions << std::endl;
                    }
                } else {
                    std::cout << "Available commands: process-smi, exit" << std::endl;
                }
            }
        } else {
            std::cout << "Usage:" << std::endl;
            std::cout << "  screen -ls                   List all processes" << std::endl;
            std::cout << "  screen -s <process_name>     Create new process screen session" << std::endl;
            std::cout << "  screen -r <process_name>     Resume existing process screen session" << std::endl;
        }
    };
    
    commands["report-util"] = [this](const std::vector<std::string>& args) {
        if (!initialized) {
            std::cout << "Please initialize the system first." << std::endl;
            return;
        }
        
        std::string filename = "logs/csopesy-log.txt";
        scheduler->generateReport(filename);
    };
    
    commands["help"] = [this](const std::vector<std::string>& args) {
        std::cout 
            << "+---------------------------------------------------------------------------------+\n"
            << "|                           CSOPESY OS Emulator Commands                          |\n"
            << "+---------------------------------------------------------------------------------+\n"
            << "|  initialize       - Initialize the processor configuration with \"config.txt\".   |\n"
            << "|  screen -s <n> - Attach or create a screen session for a process.               |\n"
            << "|  screen -r <n> - Resume an existing screen session if still running.            |\n"
            << "|       process-smi - Show process info inside screen.                            |\n"
            << "|       exit        - Exit the screen session.                                    |\n"
            << "|  screen -ls       - Show current CPU/process usage.                             |\n"
            << "|  scheduler-start  - Enable automatic dummy process generation.                  |\n"
            << "|  scheduler-test   - Start scheduler in test mode for performance testing.       |\n"
            << "|  scheduler-stop   - Disable automatic dummy process generation.                 |\n"
            << "|  report-util      - Save CPU utilization report to file.                        |\n"
            << "|  clear            - Clear the screen.                                           |\n"
            << "|  exit             - Exit the emulator.                                          |\n"
            << "+---------------------------------------------------------------------------------+\n";
    };
    
    commands["clear"] = [this](const std::vector<std::string>& args) {
        displayHeader();
    };
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
        std::cout << "\nEnter a command: ";
        
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        
        if (lowerInput == "exit") {
            if (scheduler && scheduler->isSystemRunning()) {
                std::cout << "Stopping scheduler before exit..." << std::endl;
                scheduler->stop();
            }
            break;
        }
        
        auto tokens = parseCommand(lowerInput);
        if (tokens.empty()) continue;
        
        std::string command = tokens[0];
        
        if (tokens.size() >= 2) {
            if (command == "scheduler-start" || 
               (command == "scheduler" && tokens[1] == "start")) {
                command = "scheduler-start";
            } else if (command == "scheduler-stop" || 
                      (command == "scheduler" && tokens[1] == "stop")) {
                command = "scheduler-stop";
            } else if (command == "report-util") {
                command = "report-util";
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
    std::cout << "Welcome to CSOPESY OS Emulator!" << std::endl;
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen." << std::endl;
    Utils::resetTextColor();
}
