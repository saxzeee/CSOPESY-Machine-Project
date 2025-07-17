#include "utils.h"
#include "instruction.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <random>
#include "process.h"
#include "scheduler.h"

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

std::string getCurrentTimestamp() {
    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    std::ostringstream oss;
    oss << std::put_time(&ltm, "%m/%d/%Y, %I:%M:%S %p");
    return oss.str();
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
    } else {
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
            } else {
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

        extern std::atomic<int> soloProcessCount;
        soloProcessCount++;

        std::thread soloWorker([scheduler, &screens, name, assignedCore]() {
            bool processActive = true;

            while (processActive) {
                if (!scheduler) break;

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
        << "|  screen -s <name> - Attach or create a screen session for a process.            |\n"
        << "|  screen -r <name> - Resume an existing screen session if still running.         |\n"
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

void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out, int nestLevel) {
    switch (instr.type) {
        case ICommand::InstrType::PRINT: {
            out << "PID " << proc.name << ": ";
            if (!instr.printVar.empty() && proc.variables.count(instr.printVar)) {
                out << instr.msg << " -> " << proc.variables[instr.printVar] << std::endl;
            } else {
                out << instr.msg << std::endl;
            }
            break;
        }
        case ICommand::InstrType::DECLARE: {
            proc.variables[instr.var1] = instr.value;
            out << "PID " << proc.name << ": DECLARE " << instr.var1 << " = " << instr.value << std::endl;
            break;
        }
        case ICommand::InstrType::ADD: {
            uint16_t v2 = proc.variables.count(instr.var2) ? proc.variables[instr.var2] : static_cast<uint16_t>(std::stoi(instr.var2));
            uint16_t v3 = proc.variables.count(instr.var3) ? proc.variables[instr.var3] : static_cast<uint16_t>(std::stoi(instr.var3));
            uint32_t sum = static_cast<uint32_t>(v2) + static_cast<uint32_t>(v3);
            if (sum > 65535) sum = 65535;
            proc.variables[instr.var1] = static_cast<uint16_t>(sum);
            out << "PID " << proc.name << ": ADD " << instr.var1 << " = " << v2 << " + " << v3 << " -> " << proc.variables[instr.var1] << std::endl;
            break;
        }
        case ICommand::InstrType::SUBTRACT: {
            uint16_t v2 = proc.variables.count(instr.var2) ? proc.variables[instr.var2] : static_cast<uint16_t>(std::stoi(instr.var2));
            uint16_t v3 = proc.variables.count(instr.var3) ? proc.variables[instr.var3] : static_cast<uint16_t>(std::stoi(instr.var3));
            int32_t diff = static_cast<int32_t>(v2) - static_cast<int32_t>(v3);
            if (diff < 0) diff = 0;
            proc.variables[instr.var1] = static_cast<uint16_t>(diff);
            out << "PID " << proc.name << ": SUBTRACT " << instr.var1 << " = " << v2 << " - " << v3 << " -> " << proc.variables[instr.var1] << std::endl;
            break;
        }
        case ICommand::InstrType::SLEEP: {
            out << "PID " << proc.name << ": SLEEP for " << (int)instr.sleepTicks << " ticks" << std::endl;
            if (proc.sleepRemaining == 0) { 
                proc.sleepRemaining = instr.sleepTicks;
            }
            break;
        }
        case ICommand::InstrType::FOR: {
            out << "PID " << proc.name << ": FOR loop - Nest Level: " 
                << nestLevel << ", Repeats: " << instr.repeats << std::endl;

            if (nestLevel >= 3) break;

            for (int i = 0; i < instr.repeats; ++i) {
                out << "PID " << proc.name << ": Iteration " << (i + 1) 
                    << " of " << instr.repeats << std::endl;

                for (const auto& subInstr : instr.body) {
                    executeInstruction(proc, subInstr, out, nestLevel + 1);
                }
            }
            break;
        }
    }
}

std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel) {
    std::vector<Instruction> instrs;
    std::vector<std::string> declaredVars;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> typeDist(0, nestLevel < 3 ? 5 : 4);
    std::uniform_int_distribution<> valueDist(1, 100);
    std::uniform_int_distribution<> sleepDist(1, 5);

    if (nestLevel == 0) {
        Instruction xInit;
        xInit.type = ICommand::InstrType::DECLARE;
        xInit.var1 = "x";
        xInit.value = 0;
        instrs.push_back(xInit);
        declaredVars.push_back("x");
    }

    for (int i = 0; i < count; ++i) {
        Instruction instr;
        int t = typeDist(gen);

        switch (t) {
            case 0: { // PRINT
                instr.type = ICommand::InstrType::PRINT;
                instr.msg = "Value from: " + procName;
                instr.printVar = "x";
                break;
            }
            case 1: { // DECLARE
                instr.type = ICommand::InstrType::DECLARE;
                instr.var1 = "v" + std::to_string(declaredVars.size());
                instr.value = valueDist(gen);
                declaredVars.push_back(instr.var1);
                break;
            }
            case 2: { // ADD
                instr.type = ICommand::InstrType::ADD;
                instr.var1 = "x";
                instr.var2 = "x";
                instr.var3 = "1";
                break;
            }
            case 3: { // SUBTRACT
                if (declaredVars.empty()) continue;
                instr.type = ICommand::InstrType::SUBTRACT;
                instr.var1 = "v" + std::to_string(declaredVars.size());
                instr.var2 = declaredVars[gen() % declaredVars.size()];
                if (gen() % 2 == 0 && !declaredVars.empty()) {
                    instr.var3 = declaredVars[gen() % declaredVars.size()];
                } else {
                    instr.var3 = std::to_string(valueDist(gen));
                }
                declaredVars.push_back(instr.var1);
                break;
            }
            case 4: { // SLEEP
                instr.type = ICommand::InstrType::SLEEP;
                instr.sleepTicks = sleepDist(gen);
                break;
            }
            case 5: { // FOR
                if (nestLevel >= 3) continue;
                instr.type = ICommand::InstrType::FOR;
                instr.repeats = 2 + (gen() % 3);
                instr.body = generateRandomInstructions(procName, 2, nestLevel + 1);
                break;
            }
        }
        instrs.push_back(instr);
    }
    return instrs;
}