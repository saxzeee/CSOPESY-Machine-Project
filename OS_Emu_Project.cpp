#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <random>

// Struct to hold screen session data
struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
    std::vector<std::string> sessionLog;
    std::vector<std::string> processLogs;
};

// Struct for process -> refactor into class? 
struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
    std::string startTimestamp;
    std::string finishTimestamp;
};

// Struct to hold config.txt details
struct schedConfig {
    unsigned int numCores;
    std::string algorithm;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;

};

class Scheduler {
private:
    std::vector<Process> processList;
    std::vector<int> finishedProcesses;
    std::mutex processMutex;
    bool schedulerRunning = false;
    std::thread schedulerMain;
    std::map<std::string, ScreenSession>* screenSessions = nullptr;

    void cpuWorker(int coreID) {
        while (true) {
            processMutex.lock();
            int procIndex = -1;
            for (size_t i = 0; i < processList.size(); i++) {
                if (!processList[i].finished && processList[i].executedCommands < processList[i].totalCommands) {
                    procIndex = i;
                    break;
                }
            }
            processMutex.unlock();

            if (procIndex == -1) {
                if (!schedulerRunning) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Copy data we need outside the lock to avoid holding it during I/O
            std::string procName;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                Process& proc = processList[procIndex];

                // Log before increment
                std::string logLine = "(" + getCurrentTimestamp() + ") " +
                    "Core:" + std::to_string(coreID - 1) + " " +
                    "\"Hello world from " + proc.name + "!\"";

                if (screenSessions) {
                    auto it = screenSessions->find(proc.name);
                    if (it != screenSessions->end()) {
                        it->second.processLogs.push_back(logLine);
                        it->second.currentLine = proc.executedCommands;
                    }
                }
                proc.executedCommands++;
                procName = proc.name;

                if (screenSessions) {
                    auto it = screenSessions->find(proc.name);
                    if (it != screenSessions->end()) {
                        it->second.currentLine = proc.executedCommands;
                    }
                }

                if (proc.executedCommands == proc.totalCommands) {
                    proc.finished = true;
                    proc.finishTimestamp = getCurrentTimestamp();
                    finishedProcesses.push_back(procIndex);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void cpuWorkerRoundRobin(int coreID) {
        size_t procCount = processList.size();
        size_t currentIndex = coreID - 1; // distribute processes per core start

        while (schedulerRunning) {
            processMutex.lock();

            // Find the next unfinished process in round-robin fashion
            size_t startIndex = currentIndex;
            Process* proc = nullptr;

            do {
                if (!processList[currentIndex].finished &&
                    processList[currentIndex].executedCommands < processList[currentIndex].totalCommands) {
                    proc = &processList[currentIndex];
                    break;
                }
                currentIndex = (currentIndex + 1) % procCount;
            } while (currentIndex != startIndex);

            processMutex.unlock();

            if (!proc) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            int cyclesToRun = std::min((int)quantumCycles,
                                    proc->totalCommands - proc->executedCommands);

            for (int i = 0; i < cyclesToRun; ++i) {
                processMutex.lock();

                if (proc->finished) {
                    processMutex.unlock();
                    break;
                }

                std::string logLine = "(" + getCurrentTimestamp() + ") Core:" +
                    std::to_string(coreID - 1) + " \"Hello world from " + proc->name + "!\"";

                if (screenSessions) {
                    auto it = screenSessions->find(proc->name);
                    if (it != screenSessions->end()) {
                        it->second.processLogs.push_back(logLine);
                        it->second.currentLine = proc->executedCommands;
                    }
                }

                proc->executedCommands++;
                if (proc->executedCommands >= proc->totalCommands) {
                    proc->finished = true;
                    proc->finishTimestamp = getCurrentTimestamp();
                    finishedProcesses.push_back(currentIndex);
                }

                processMutex.unlock();

                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
            }

            currentIndex = (currentIndex + 1) % procCount;
        }
    }
    
    // edit
    void scheduler() {
        while (true) {
            processMutex.lock();
            bool allDone = true;
            for (size_t i = 0; i < processList.size(); i++) {
                if (!processList[i].finished) {
                    allDone = false;
                    break;
                }
            }
            processMutex.unlock();
            if (allDone) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        schedulerRunning = false;
    }

    std::string getCurrentTimestamp() {
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        std::ostringstream oss;
        oss << std::put_time(&ltm, "%m/%d/%Y, %I:%M:%S %p");
        return oss.str();
    }

public:
    unsigned int numCores;
    std::string algorithm;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;

	Scheduler(const schedConfig& config) {
	    numCores = config.numCores;
	    algorithm = config.algorithm;
	    quantumCycles = config.quantumCycles;
	    batchProcFreq = config.batchProcFreq;
	    minIns = config.minIns;
	    maxIns = config.maxIns;
	    delayPerExec = config.delayPerExec;
	}

    void addProcess(const Process& process) {
        processList.push_back(process);
    }

    void startScheduler() {
        if (schedulerRunning) {
            std::cout << "Scheduler already running.\n";
            return;
        }
        schedulerRunning = true;
        schedulerMain = std::thread(&Scheduler::runScheduler, this);
    }

    bool isRunning() const {
        return schedulerRunning;
    }

    void runScheduler() {
        // create processes each X CPU ticks
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(minIns, maxIns);
        int processCounter = 1;

        while (schedulerRunning && processCounter <= 10) {
            Process proc;
            proc.name = "process" + std::to_string(processCounter++);
            proc.totalCommands = dist(gen);  
            proc.executedCommands = 0;
            proc.startTimestamp = getCurrentTimestamp(); 
            proc.finished = false;

            processMutex.lock();
            processList.push_back(proc);
            if (screenSessions) {
                (*screenSessions)[proc.name] = {
                    proc.name, 1, proc.totalCommands, proc.startTimestamp
                };
            }
            processMutex.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(batchProcFreq));
        }

        std::vector<std::thread> cpuThreads;
        /*
        for (int i = 1; i <= static_cast<int>(numCores); i++) {
             cpuThreads.push_back(std::thread(&Scheduler::cpuWorker, this, i));
        }
        */
        for (int i = 1; i <= static_cast<int>(numCores); i++) {
            if (algorithm == "rr") {
                cpuThreads.emplace_back(&Scheduler::cpuWorkerRoundRobin, this, i);
            } else if (algorithm == "fcfs"){
                cpuThreads.emplace_back(&Scheduler::cpuWorker, this, i);
            }
            else {
                std::cout << "Algorithm Invalid";
            }
        }

        std::thread schedThread(&Scheduler::scheduler, this);

        schedThread.join();
		for (size_t i = 0; i < cpuThreads.size(); i++) {
		    cpuThreads[i].join();
		}
    }

    void shutdown() {
        schedulerRunning = false;
        if (schedulerMain.joinable()) {
            schedulerMain.join();
        }
    }

    void printConfig() const { // display config for verifying if it creates and assigns attributes correctly
        std::cout << "---- Scheduler Configuration ----\n";
        std::cout << "Number of CPU Cores   : " << numCores << "\n";
        std::cout << "Scheduling Algorithm  : " << algorithm << "\n";
        std::cout << "Quantum Cycles        : " << quantumCycles << "\n";
        std::cout << "Batch Process Freq    : " << batchProcFreq << "\n";
        std::cout << "Min Instructions      : " << minIns << "\n";
        std::cout << "Max Instructions      : " << maxIns << "\n";
        std::cout << "Delay per Execution   : " << delayPerExec << "\n";
        std::cout << "----------------------------------\n";
    }

    // show process status
    void showScreenLS() {
        processMutex.lock();
        std::cout << std::left; // Align text to the left
        int nameWidth = 12;

        std::cout << "---------------------------------------------\n";
        std::cout << "Running processes:\n";
        for (size_t i = 0; i < processList.size(); i++) {
            if (!processList[i].finished) {
                std::cout << std::setw(nameWidth) << processList[i].name << "  ";
                std::cout << "(Started: " << processList[i].startTimestamp << ")  ";
                std::cout << "Core: " << (i % 4) << "  ";
                std::cout << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
            }
        }
        std::cout << "\nFinished processes:\n";
        for (size_t i = 0; i < processList.size(); i++) {
            if (processList[i].finished) {
                std::cout << std::setw(nameWidth) << processList[i].name << "  ";
                std::cout << "(" << processList[i].finishTimestamp << ")  ";
                std::cout << "Finished  ";
                std::cout << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
            }
        }
        std::cout << "---------------------------------------------\n";
        processMutex.unlock();
    }
    void setScreenMap(std::map<std::string, ScreenSession>* screens) {
        screenSessions = screens;
     }
    
};
// global variables for process
std::vector<Process> processList;
std::vector<int> finishedProcesses;
std::mutex processMutex;

// Cross-platform clear screen
void clearScreen() {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

// Utility to set terminal text color (ANSI)
void setTextColor(int colorID) {
    std::cout << "\033[" << colorID << "m";
}

void defaultColor() {
    std::cout << "\033[0m";
}

// ASCII Header Display
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

// get current timestamp
std::string getCurrentTimestamp() {
    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    std::ostringstream oss;
    oss << std::put_time(&ltm, "%m/%d/%Y, %I:%M:%S %p");
    return oss.str();
}

// Display screen layout
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
    //std::cout << "\n(Type 'exit' to return to the main menu)\n";
}

// Loop for inside screen session
void screenLoop(ScreenSession& session) {
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
            std::stringstream ss;
            if (!session.processLogs.empty()) {
                std::cout << "\n--- Process Logs ---\n";
                for (const auto& log : session.processLogs) {
                    std::cout << log << "\n";
                }
            }
            else {
                std::cout << "No logs found for this process.\n";
            }

            for (const auto& proc : processList) {
                if (proc.name == session.name && proc.finished) {
                    std::cout << "Finished!\n";
                    break;
                }
            }
        }
    }
}

// Base instruction interface
class ICommand {
public:
    enum CommandType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };
    ICommand(int pid, CommandType type) : pid(pid), commandType(type) {}
    virtual ~ICommand() = default;
    CommandType getCommandType() { return commandType; }
    // Pass symbol table to each execute
    virtual void execute(std::map<std::string, uint16_t>& symbols, std::ostream& out = std::cout) = 0;

protected:
    int pid;
    CommandType commandType;
};

// PRINT command
class PrintCommand : public ICommand {
public:
    // If varName is empty, just print msg. If not, print msg + value of varName.
    PrintCommand(int pid, const std::string& msg, const std::string& varName = "")
        : ICommand(pid, PRINT), msg(msg), varName(varName) {}

    void execute(std::map<std::string, uint16_t>& symbols, std::ostream& out = std::cout) override {
        out << "PID " << pid << ": ";
        if (!varName.empty()) {
            uint16_t val = 0;
            if (symbols.count(varName)) val = symbols[varName];
            out << msg << val << std::endl;
        } else {
            out << msg << std::endl;
        }
    }

private:
    std::string msg;
    std::string varName;
};

// DECLARE command
class DeclareCommand : public ICommand {
public:
    DeclareCommand(int pid, const std::string& varName, uint16_t value)
        : ICommand(pid, DECLARE), varName(varName), value(value) {}

    void execute(std::map<std::string, uint16_t>& symbols, std::ostream& out = std::cout) override {
        symbols[varName] = value;
        out << "PID " << pid << ": DECLARE " << varName << " = " << value << std::endl;
    }

private:
    std::string varName;
    uint16_t value;
};

// Helper: get value from symbol table or parse as uint16_t
inline uint16_t getVarOrValue(const std::string& s, std::map<std::string, uint16_t>& symbols) {
    if (std::all_of(s.begin(), s.end(), ::isdigit)) {
        return static_cast<uint16_t>(std::stoi(s));
    }
    // If not found, auto-declare as 0
    if (!symbols.count(s)) symbols[s] = 0;
    return symbols[s];
}

// ADD command
class AddCommand : public ICommand {
public:
    AddCommand(int pid, const std::string& dest, const std::string& src1, const std::string& src2)
        : ICommand(pid, ADD), dest(dest), src1(src1), src2(src2) {}

    void execute(std::map<std::string, uint16_t>& symbols, std::ostream& out = std::cout) override {
        uint16_t v1 = getVarOrValue(src1, symbols);
        uint16_t v2 = getVarOrValue(src2, symbols);
        uint32_t sum = static_cast<uint32_t>(v1) + static_cast<uint32_t>(v2);
        if (sum > 65535) sum = 65535; // clamp to uint16_t
        symbols[dest] = static_cast<uint16_t>(sum);
        out << "PID " << pid << ": ADD " << dest << " = " << v1 << " + " << v2 << " -> " << symbols[dest] << std::endl;
    }

private:
    std::string dest, src1, src2;
};

// SUBTRACT command
class SubtractCommand : public ICommand {
public:
    SubtractCommand(int pid, const std::string& dest, const std::string& src1, const std::string& src2)
        : ICommand(pid, SUBTRACT), dest(dest), src1(src1), src2(src2) {}

    void execute(std::map<std::string, uint16_t>& symbols, std::ostream& out = std::cout) override {
        uint16_t v1 = getVarOrValue(src1, symbols);
        uint16_t v2 = getVarOrValue(src2, symbols);
        int32_t diff = static_cast<int32_t>(v1) - static_cast<int32_t>(v2);
        if (diff < 0) diff = 0; // clamp to 0
        symbols[dest] = static_cast<uint16_t>(diff);
        out << "PID " << pid << ": SUBTRACT " << dest << " = " << v1 << " - " << v2 << " -> " << symbols[dest] << std::endl;
    }

private:
    std::string dest, src1, src2;
};

// SLEEP command
class SleepCommand : public ICommand {
public:
    SleepCommand(int pid, uint8_t ticks) : ICommand(pid, SLEEP), ticks(ticks) {}

    void execute(std::map<std::string, uint16_t>&, std::ostream& out = std::cout) override {
        out << "PID " << pid << ": SLEEP for " << (int)ticks << " ticks" << std::endl;
        // Actual sleep logic should be handled by the scheduler
    }

private:
    uint8_t ticks;
};

// FOR command (can be nested)
class ForCommand : public ICommand {
public:
    ForCommand(int pid, std::vector<std::unique_ptr<ICommand>>& body, int repeats)
        : ICommand(pid, FOR), repeats(repeats) {
        // Deep copy/move the body
        for (auto& cmd : body) {
            body_.emplace_back(std::move(cmd));
        }
    }

    void execute(std::map<std::string, uint16_t>& symbols, std::ostream& out = std::cout) override {
        out << "PID " << pid << ": FOR loop x" << repeats << std::endl;
        for (int i = 0; i < repeats; ++i) {
            for (auto& cmd : body_) {
                cmd->execute(symbols, out);
            }
        }
    }

private:
    std::vector<std::unique_ptr<ICommand>> body_;
    int repeats;
};

bool readConfigFile(std::string filePath, schedConfig* config) { //read the configs and edit the pased config struct that holds the details
    std::ifstream file(filePath);
    if (!file.is_open()) { // simple error cheking
        std::cerr << "Error opening config file\n";
        return false;
    }
    std::string fileLine;
    while (std::getline(file, fileLine)) {
        std::istringstream iss(fileLine);
        std::string attribute;
        if (!(iss >> attribute)) continue;

        if (attribute == "scheduler") {
            std::string schedValue;
            if (iss >> std::quoted(schedValue)) {
                config->algorithm = schedValue;
            }
            else {
                std::cerr << "Error reading algo\n";
            }
        }
        else {
            std::string configVals;
            if (!(iss >> configVals)) continue;
            if (attribute == "num-cpu") {
                config->numCores = std::stoi(configVals);
            }
            else if (attribute == "quantum-cycles") {
                config->quantumCycles = std::stoul(configVals);
            }
            else if (attribute == "batch-process-freq") {
                config->batchProcFreq = std::stoul(configVals);
            }
            else if (attribute == "min-ins") {
                config->minIns = std::stoul(configVals);
            }
            else if (attribute == "max-ins") {
                config->maxIns = std::stoul(configVals);
            }
            else if (attribute == "delay-per-exec") {
                config->delayPerExec = std::stoul(configVals);
            }
        }
    }
    return true;
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
    for (const auto& proc : processList) {
        if (proc.name == name) {
            found = true;
            finished = proc.finished;
            break;
        }
    }

    if (finished) {
        std::cout << "Process '" << name << "' has already finished.\n";
        return;
    }

    if (found) {
        screenLoop(screens[name]);
        return;
    }

    // Generate a random instruction count within configured bounds
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(scheduler->minIns, scheduler->maxIns);
    int randomInstructions = dist(gen);

    // Create new process with randomized totalCommands
    Process proc;
    proc.name = name;
    proc.totalCommands = randomInstructions;
    proc.executedCommands = 0;
    proc.finished = false;

    screens[name] = { name, 1, proc.totalCommands, getCurrentTimestamp() };

    scheduler->addProcess(proc);
}

void handleScreenR(const std::string& name, std::map<std::string, ScreenSession>& screens) {
    auto it = screens.find(name);
    if (it == screens.end()) {
        std::cout << "Process " << name << " not found.\n";
        return;
    }

    for (const auto& proc : processList) {
        if (proc.name == name && proc.finished) {
            std::cout << "Process " << name << " has already finished.\n";
            return;
        }
    }
    screenLoop(it->second);
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

void handleReportUtil() {
    std::cout << "report-util command recognized. (Not implemented)\n";
}

void handleHelp() {
    std::cout << "Commands:\n"
              << "  initialize       - Initialize the processor configuration with \"config.txt\".\n"
              << "  screen -s <name> - Attach or create a screen session for a process.\n"
              << "  screen -r <name> - Resume an existing screen session if still running.\n"
              << "       process-smi - Show process info inside screen.\n"
              << "       exit        - Exit the screen session.\n"
              << "  screen -ls       - Show current CPU/process usage.\n"
              << "  scheduler-start  - Start dummy process generation.\n"
              << "  scheduler-stop   - Stop process generation (not yet implemented).\n"
              << "  report-util      - Save CPU utilization report to file.\n"
              << "  clear            - Clear the screen.\n"
              << "  exit             - Exit the emulator.\n";
}

// for testing processes
// void simulateTwoProcesses() {
//     // Simulate two processes with their own symbol tables and instruction lists
//     int pid1 = 1, pid2 = 2;
//     std::map<std::string, uint16_t> symbols1, symbols2;
//     std::vector<std::unique_ptr<ICommand>> instructions1;
//     std::vector<std::unique_ptr<ICommand>> instructions2;

//     // Process 1 instructions
//     instructions1.emplace_back(std::make_unique<DeclareCommand>(pid1, "y", 15));
//     instructions1.emplace_back(std::make_unique<AddCommand>(pid1, "x", "x", "5")); // x = x + 5
//     instructions1.emplace_back(std::make_unique<PrintCommand>(pid1, "Value of x: ", "x"));
//     instructions1.emplace_back(std::make_unique<SleepCommand>(pid1, 2));
//     instructions1.emplace_back(std::make_unique<SubtractCommand>(pid1, "x", "y", "3")); // x = x - 3
//     instructions1.emplace_back(std::make_unique<PrintCommand>(pid1, "Final x: ", "x"));

//     // Process 2 instructions
//     instructions2.emplace_back(std::make_unique<DeclareCommand>(pid2, "y", 20));
//     instructions2.emplace_back(std::make_unique<AddCommand>(pid2, "y", "y", "7")); // y = y + 7
//     instructions2.emplace_back(std::make_unique<PrintCommand>(pid2, "Value of y: ", "y"));
//     instructions2.emplace_back(std::make_unique<SleepCommand>(pid2, 1));
//     instructions2.emplace_back(std::make_unique<SubtractCommand>(pid2, "y", "y", "15")); // y = y - 15
//     instructions2.emplace_back(std::make_unique<PrintCommand>(pid2, "Final y: ", "y"));

//     // Simulate running each process step by step
//     std::cout << "=== Simulating Process 1 ===\n";
//     for (auto& cmd : instructions1) {
//         cmd->execute(symbols1);
//     }

//     std::cout << "\n=== Simulating Process 2 ===\n";
//     for (auto& cmd : instructions2) {
//         cmd->execute(symbols2);
//     }
// }

int main() {
    bool menuState = true;
    bool initialized = false;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;
    schedConfig config;
    dispHeader();
    std::unique_ptr<Scheduler> procScheduler; //unique ptr for process scheduler where it will be created & config will be assigned later, also unique_ptr for memory management
    // for the headers
    /* 
    for (auto& proc : processList) {
        std::string filename = proc.name + ".txt";
        std::ofstream outfile(filename, std::ios::trunc);
        outfile << "Process name: " << proc.name << std::endl;
        outfile << "Logs:" << std::endl << std::endl;
        outfile.close();
    } 

    std::vector<std::thread> cpuThreads;
    for (int i = 1; i <= 4; i++) {
        cpuThreads.push_back(std::thread(cpuWorker, i));
    }

    std::thread schedThread(scheduler);
    */
    // simulateTwoProcesses(); 
    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        // Convert to lowercase
        std::transform(inputCommand.begin(), inputCommand.end(), inputCommand.begin(), ::tolower);

        if (inputCommand.find("initialize") != std::string::npos) {
            // initialize -> read config.txt file and setup scheduler details using the given config
            handleInitialize(config, procScheduler, screens, initialized);
        }
        else if (inputCommand == "-help") {
            handleHelp();
        }
        else if (inputCommand == "clear") {
            clearScreen();
            dispHeader();
        }
        else if (inputCommand == "exit") {
            procScheduler->shutdown();
            menuState = false;
            /*
            std::cout << "Waiting for all processes to finish...\n";
            schedThread.join();
            for (int i = 0; i < cpuThreads.size(); i++) {
                cpuThreads[i].join();
            }
            std::cout << "All processes finished. Exiting emulator.\n";
            */
        }
        else if (!initialized) {
            std::cout << "Run the initialize command first before proceeding.\n";
        }
        else if (inputCommand.rfind("screen -s ", 0) == 0) {
            handleScreenS(inputCommand.substr(10), procScheduler.get(), screens);
        }
        else if (inputCommand.rfind("screen -r ", 0) == 0) {
            handleScreenR(inputCommand.substr(10), screens);
        }
        else if (inputCommand.find("screen -ls") != std::string::npos) {
            procScheduler->showScreenLS();
        }
        else if (inputCommand.find("scheduler-start") != std::string::npos) {
            procScheduler->startScheduler();
        }
        else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            handleSchedulerStop(procScheduler);
        }
        else if (inputCommand.find("report-util") != std::string::npos) {
            handleReportUtil();
        } else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }
        inputCommand = "";
    }
    return 0;
}