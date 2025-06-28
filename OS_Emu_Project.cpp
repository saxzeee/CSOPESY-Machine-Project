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

struct Instruction; // Forward declaration

// Struct for process -> refactor into class? 
struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
    std::string startTimestamp;
    std::string finishTimestamp;
    std::vector<Instruction> instructions; // Add this
    std::map<std::string, uint16_t> variables; // For process memory
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

void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out = std::cout, int nestLevel = 0);
std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel);

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
                std::ostringstream ss;
                ss << "(" << getCurrentTimestamp() << ") Core:" << (coreID - 1) << " ";
                if (proc.executedCommands < proc.instructions.size()) {
                    Instruction& instr = proc.instructions[proc.executedCommands];
                    executeInstruction(proc, instr, ss);  // this logs the real instruction
                }
                std::string logLine = ss.str();

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
        size_t currentIndex = coreID - 1;

        while (schedulerRunning) {
            size_t procIndex = -1;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                size_t procCount = processList.size();
                if (procCount == 0) continue;

                size_t startIndex = currentIndex;
                do {
                    if (currentIndex >= procCount) currentIndex = 0;
                    if (!processList[currentIndex].finished &&
                        processList[currentIndex].executedCommands < processList[currentIndex].totalCommands) {
                        procIndex = currentIndex;
                        break;
                    }
                    currentIndex = (currentIndex + 1) % procCount;
                } while (currentIndex != startIndex);
            }

            if (procIndex == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            for (int i = 0; i < (int)quantumCycles; ++i) {
                std::lock_guard<std::mutex> lock(processMutex);
                if (procIndex >= processList.size()) break;  // Sanity check

                Process& proc = processList[procIndex];
                if (proc.finished || proc.executedCommands >= proc.totalCommands)
                    break;

                std::ostringstream ss;
                ss << "(" << getCurrentTimestamp() << ") Core:" << (coreID - 1) << " ";
                if (proc.executedCommands < proc.instructions.size()) {
                    Instruction& instr = proc.instructions[proc.executedCommands];
                    executeInstruction(proc, instr, ss);
                }
                std::string logLine = ss.str();

                if (screenSessions) {
                    auto it = screenSessions->find(proc.name);
                    if (it != screenSessions->end()) {
                        it->second.processLogs.push_back(logLine);
                        it->second.currentLine = proc.executedCommands;
                    }
                }

                proc.executedCommands++;
                if (proc.executedCommands >= proc.totalCommands) {
                    proc.finished = true;
                    proc.finishTimestamp = getCurrentTimestamp();
                    finishedProcesses.push_back(procIndex);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
            }

            currentIndex = (currentIndex + 1) % processList.size();
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
        // Start CPU threads first
        std::vector<std::thread> cpuThreads;
        for (int i = 1; i <= static_cast<int>(numCores); i++) {
            if (algorithm == "rr") {
                cpuThreads.emplace_back(&Scheduler::cpuWorkerRoundRobin, this, i);
            }
            else if (algorithm == "fcfs") {
                cpuThreads.emplace_back(&Scheduler::cpuWorker, this, i);
            }
            else {
                std::cout << "Algorithm Invalid\n";
                return;
            }
        }
        // Process generator
        std::thread processCreator([this]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(minIns, maxIns);
            int processCounter = 1;

            while (schedulerRunning) {
                Process proc;
                proc.name = "process" + std::to_string(processCounter++);
                proc.totalCommands = dist(gen);
                proc.executedCommands = 0;
                proc.startTimestamp = getCurrentTimestamp();
                proc.finished = false;

                proc.instructions = generateRandomInstructions(proc.name, proc.totalCommands, 0);

                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    processList.push_back(proc);
                    if (screenSessions) {
                        (*screenSessions)[proc.name] = {
                            proc.name, 1, proc.totalCommands, proc.startTimestamp
                        };
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(batchProcFreq));
            }
            });

        // Check for processes if done
        std::thread schedThread(&Scheduler::scheduler, this);

        // Wait for processes to end
        processCreator.join();
        schedThread.join();
        for (auto& t : cpuThreads) t.join();
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




struct ICommand {
    enum class InstrType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };
};

struct Instruction {
    ICommand::InstrType type;
    std::string msg;
    std::string printVar;
    std::string var1, var2, var3;
    uint16_t value = 0;
    uint8_t sleepTicks = 0;
    std::vector<Instruction> body;
    int repeats = 0;
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
    }
    else {
        std::cout << "Initialization failed.\n";
    }
}

std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel = 0);

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
    proc.instructions = generateRandomInstructions(name, randomInstructions, 0);

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

void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out, int nestLevel) {
    //std::cout << "[DEBUG] Executing " << static_cast<int>(instr.type) << " for " << proc.name << std::endl;

    switch (instr.type) {
        case ICommand::InstrType::PRINT: {
            out << "PID " << proc.name << ": ";
            out << instr.msg;
            if (!instr.printVar.empty()) {
                uint16_t val = proc.variables[instr.printVar];
                out << val;
            }
            out << std::endl;
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
            break;
        }
        case ICommand::InstrType::FOR: {
            out << "PID " << proc.name << ": FOR loop x" << instr.repeats << std::endl;
            if (nestLevel >= 3) break; // Max 3 nested
            for (int i = 0; i < instr.repeats; ++i) {
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

    for (int i = 0; i < count; ++i) {
        Instruction instr;
        int t = typeDist(gen);

        switch (t) {
            case 0: { // PRINT
                instr.type = ICommand::InstrType::PRINT;
                instr.msg = "Hello world from " + procName + "!";
                if (!declaredVars.empty() && gen() % 2 == 0) {
                    instr.printVar = declaredVars[gen() % declaredVars.size()];
                }
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
                if (declaredVars.empty()) continue;

                instr.type = ICommand::InstrType::ADD;
                instr.var1 = "v" + std::to_string(declaredVars.size()); // new variable
                instr.var2 = declaredVars[gen() % declaredVars.size()];

                if (gen() % 2 == 0 && !declaredVars.empty()) {
                    instr.var3 = declaredVars[gen() % declaredVars.size()];
                } else {
                    instr.var3 = std::to_string(valueDist(gen));
                }

                declaredVars.push_back(instr.var1);
                break;
            }

            case 3: { // SUBTRACT
                if (declaredVars.empty()) continue;

                instr.type = ICommand::InstrType::SUBTRACT;
                instr.var1 = "v" + std::to_string(declaredVars.size()); // new variable
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
                if (nestLevel >= 3) continue; // prevent exceeding max nesting
                instr.type = ICommand::InstrType::FOR;
                instr.repeats = 2 + (gen() % 3); // repeat 2–4 times
                instr.body = generateRandomInstructions(procName, 2, nestLevel + 1);
                break;
            }
        }

        instrs.push_back(instr);
    }

    return instrs;
}

int main() {
    bool menuState = true;
    bool initialized = false;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;
    schedConfig config;
    dispHeader();
    std::unique_ptr<Scheduler> procScheduler; //unique ptr for process scheduler where it will be created & config will be assigned later, also unique_ptr for memory management

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
            if (procScheduler && procScheduler->isRunning()) {
                std::cout << "Stopping scheduler before exit...\n";
                procScheduler->shutdown();
            }
            menuState = false;
            procScheduler.reset();
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
        }
        else {
            std::cout << "Command not recognized. Type \"-help\" to display commands.\n";
        }
        inputCommand = "";
    }
    return 0;
}