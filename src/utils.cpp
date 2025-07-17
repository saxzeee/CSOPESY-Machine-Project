#include "utils.h"
#include "instruction.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <random>

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
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&ltm, &now);
#else
    localtime_r(&now, &ltm);
#endif
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