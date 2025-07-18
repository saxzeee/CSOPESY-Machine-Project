#include "headers/scheduler.h"
#include "headers/process.h"
#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <thread>
#include <chrono>
#include <memory>
#include <random>
#include <sstream>

void clearScreen() {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

void setTextColor(int colorID) {
    std::cout << "\033[" << colorID << "m";
}

void defaultColor() {
    std::cout << "\033[0m";
}

void dispHeader() {
    std::string opesy_ascii = R"(
     _/_/_/    _/_/_/    _/_/    _/_/_/    _/_/_/_/    _/_/_/  _/      _/  
  _/        _/        _/    _/  _/    _/  _/        _/          _/  _/     
 _/          _/_/    _/    _/  _/_/_/    _/_/_/      _/_/        _/        
_/              _/  _/    _/  _/        _/              _/      _/         
 _/_/_/  _/_/_/      _/_/    _/        _/_/_/_/  _/_/_/        _/                                                                               
)";
    setTextColor(34);
    std::cout << opesy_ascii << std::endl;
    setTextColor(32);
    std::cout << "Welcome to CSOPESY OS Emulator!\n";
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen.\n";
    defaultColor();
}

void displayScreen(const ScreenSession& session) {
    setTextColor(36);
    std::cout << "Process name: " << session.name << "\n";
    std::cout << "Instruction: Line " << session.currentLine << " / " << session.totalLines << "\n";
    std::cout << "Created at: " << session.timestamp << "\n";
    defaultColor();
    if (!session.sessionLog.empty()) {
        for (const auto& entry : session.sessionLog) {
            std::cout << entry << "\n";
        }
    }
}

void screenLoop(ScreenSession& session, Scheduler* scheduler) {
    std::string input;
    clearScreen();

    while (true) {
        displayScreen(session);
        std::cout << "\n>> ";
        std::getline(std::cin, input);
        if (input == "exit") {
            clearScreen();
            dispHeader();
            break;
        }
        else if (input == "process-smi") {
            if (!session.processLogs.empty()) {
                std::cout << "\n--- Process Logs ---\n";
                for (const auto& log : session.processLogs) {
                    std::cout << log << "\n";
                }
            }
            else {
                std::cout << "No logs found for this process.\n";
            }

            for (const auto& proc : scheduler->getProcessList()) {
                if (proc.name == session.name && proc.finished) {
                    std::cout << "Finished!\n";
                    break;
                }
            }
        }
    }
}

void handleInitialize(schedConfig& config, std::unique_ptr<Scheduler>& scheduler, std::map<std::string, ScreenSession>& screens, bool& initialized) {
    if (readConfigFile("config.txt", &config)) {
        scheduler = std::make_unique<Scheduler>(config);
        scheduler->printConfig();
        scheduler->setScreenMap(&screens);
        initialized = true;
    }
    else {
        std::cout << "Initialization failed.\n";
    }
}

void handleScreenS(const std::string& name, Scheduler* scheduler, std::map<std::string, ScreenSession>& screens) {
    bool found = false, finished = false;

    for (const auto& proc : scheduler->getProcessList()) {
        if (proc.name == name) {
            found = true;
            finished = proc.finished;

            if (finished) {
                std::cout << "Process '" << name << "' has already finished.\n";
            }
            else {
                screenLoop(screens[name], scheduler);
            }
            return;
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(scheduler->minIns, scheduler->maxIns);
    int randomInstructions = dist(gen);

    Process proc;
    proc.name = name;
    proc.totalCommands = randomInstructions;
    proc.executedCommands = 0;
    proc.finished = false;
    proc.coreAssigned = -1;
    proc.startTimestamp = getCurrentTimestamp();
    proc.instructions = generateRandomInstructions(proc.name, proc.totalCommands, 0);

    if (!scheduler->addProcess(proc)) {
        std::cout << "Process creation failed due to insufficient memory.\n";
        return;
    }
    screens[name] = { name, 1, proc.totalCommands, proc.startTimestamp };

    if (!scheduler->isRunning()) {
        auto& coreToProcess = scheduler->getCoreToProcess();
        int assignedCore = -1;

        for (int i = 0; i < scheduler->numCores; ++i) {
            if (coreToProcess[i].empty()) {
                assignedCore = i;
                break;
            }
        }

        if (assignedCore == -1) {
            std::cout << "Cannot start '" << name << "': no available cores for solo screen processes.\n";
            return;
        }

        soloProcessCount++;

        std::thread soloWorker([scheduler, &screens, name, assignedCore, &soloProcessCount]() {
            bool processActive = true;
            
            while (processActive) {
                if (!scheduler) {
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(scheduler->getProcessMutex());
                    auto& procList = const_cast<std::vector<Process>&>(scheduler->getProcessList());
                    auto& coreToProcess = scheduler->getCoreToProcess();

                    bool processFound = false;
                    for (auto& proc : procList) {
                        if (proc.name == name && !proc.finished &&
                            proc.executedCommands < proc.totalCommands &&
                            proc.coreAssigned == -1) {

                            processFound = true;
                            proc.coreAssigned = assignedCore;
                            coreToProcess[assignedCore] = proc.name;

                            std::ostringstream ss;
                            ss << "(" << getCurrentTimestamp() << ") Core:" << assignedCore << " ";
                            std::string logLine;

                            if (proc.executedCommands < proc.instructions.size()) {
                                Instruction& instr = proc.instructions[proc.executedCommands];
                                executeInstruction(proc, instr, ss);
                                logLine = ss.str();

                                auto it = screens.find(proc.name);
                                if (it != screens.end()) {
                                    it->second.processLogs.push_back(logLine);
                                    it->second.currentLine = proc.executedCommands;
                                }
                            }

                            proc.executedCommands++;

                            if (proc.executedCommands >= proc.totalCommands) {
                                proc.finished = true;
                                proc.finishTimestamp = getCurrentTimestamp();
                                coreToProcess[assignedCore] = "";
                                if (proc.memoryAddress != -1) {
                                    scheduler->getProcessMutex().unlock();
                                    scheduler->freeProcessMemory(proc.name);
                                    scheduler->getProcessMutex().lock();
                                }
                                soloProcessCount--;
                                processActive = false; 
                            }

                            proc.coreAssigned = -1;
                            break;
                        }
                    }
                    
                    if (!processFound) {
                        coreToProcess[assignedCore] = "";
                        soloProcessCount--;
                        processActive = false;
                    }
                }

                if (processActive) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(scheduler->delayPerExec));
                }
            }
        });

        soloWorker.detach(); 
    }

    screenLoop(screens[name], scheduler);
}

void handleScreenR(const std::string& name, std::map<std::string, ScreenSession>& screens, Scheduler* scheduler) {
    auto it = screens.find(name);
    if (it == screens.end()) {
        std::cout << "Process " << name << " not found.\n";
        return;
    }

    for (const auto& proc : scheduler->getProcessList()) {
        if (proc.name == name && proc.finished) {
            std::cout << "Process " << name << " has already finished.\n";
            it->second.currentLine = it->second.totalLines;
            return;
        }
        if (proc.name == name) {
            it->second.currentLine = proc.executedCommands;
            break;
        }
    }
    screenLoop(it->second, scheduler);
}

void handleSchedulerStop(std::unique_ptr<Scheduler>& scheduler) {
    if (!scheduler) {
        std::cout << "Scheduler is not initialized.\n";
        return;
    }

    if (!scheduler->isRunning()) {
        std::cout << "Scheduler is not currently running.\n";
        return;
    }

    scheduler->shutdown();
    std::cout << "Scheduler stopped successfully.\n";
}

void handleHelp() {
    std::cout 
        << "+---------------------------------------------------------------------------------+\n"
        << "|                           CSOPESY OS Emulator Commands                          |\n"
        << "+---------------------------------------------------------------------------------+\n"
        << "|  initialize       - Initialize the processor configuration with \"config.txt\".   |\n"
        << "|  screen -s <n> - Attach or create a screen session for a process.            |\n"
        << "|  screen -r <n> - Resume an existing screen session if still running.         |\n"
        << "|       process-smi - Show process info inside screen.                            |\n"
        << "|       exit        - Exit the screen session.                                    |\n"
        << "|  screen -ls       - Show current CPU/process usage.                             |\n"
        << "|  scheduler-start  - Start dummy process generation.                             |\n"
        << "|  scheduler-stop   - Stop process generation and free memory.                    |\n"
        << "|  report-util      - Save CPU utilization report to file.                        |\n"
        << "|  clear            - Clear the screen.                                           |\n"
        << "|  exit             - Exit the emulator.                                          |\n"
        << "+---------------------------------------------------------------------------------+\n";
}

int main() {
    bool menuState = true;
    bool initialized = false;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;
    schedConfig config;
    dispHeader();
    std::unique_ptr<Scheduler> procScheduler;

    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        std::transform(inputCommand.begin(), inputCommand.end(), inputCommand.begin(), ::tolower);

        if (inputCommand.find("initialize") != std::string::npos) {
            handleInitialize(config, procScheduler, screens, initialized);
        } else if (inputCommand == "help") {
            handleHelp();
        } else if (inputCommand == "clear") {
            clearScreen();
            dispHeader();
        } else if (inputCommand == "exit") {
            if (procScheduler && procScheduler->isRunning()) {
                std::cout << "Stopping scheduler before exit...\n";
                procScheduler->shutdown();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            while (soloProcessCount > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            menuState = false;
            procScheduler.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else if (!initialized) {
            std::cout << "Run the initialize command first before proceeding.\n";
        } else if (inputCommand.rfind("screen -s ", 0) == 0) {
            handleScreenS(inputCommand.substr(10), procScheduler.get(), screens);
        } else if (inputCommand.rfind("screen -r ", 0) == 0) {
            handleScreenR(inputCommand.substr(10), screens, procScheduler.get());
        } else if (inputCommand.find("screen -ls") != std::string::npos) {
            procScheduler->showScreenLS();
        } else if (inputCommand.find("scheduler-start") != std::string::npos) {
            procScheduler->startScheduler();
        } else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            handleSchedulerStop(procScheduler);
        } else if (inputCommand.find("report-util") != std::string::npos) {
            if (procScheduler) {
                procScheduler->generateLog("csopesy-log.txt");
                procScheduler->printMemory();
            } else {
                std::cout << "Scheduler not initialized.\n";
            }
        } else if (inputCommand == "mem-status") {
            if (procScheduler) {
                procScheduler->printMemory();
            } else {
                std::cout << "Scheduler not initialized.\n";
            }
        } else {
            std::cout << "Command not recognized. Type 'help' to display commands.\n";
        }
        inputCommand = "";
    }
    
    return 0;
}
