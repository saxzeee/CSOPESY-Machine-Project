#include <filesystem>
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
#include <atomic>
#include <deque>

static std::atomic<int> soloProcessCount(0); 

// Struct to hold screen session data
struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
    std::vector<std::string> sessionLog;
    std::vector<std::string> processLogs;
};

struct Instruction;

// Struct for memory block (for first-fit allocator)
struct MemoryBlock {
    size_t start;
    size_t size;
    bool allocated;
    std::string owner; // process name or empty
};

// Memory manager for first-fit allocation
class MemoryManager {
public:
    // Returns the number of processes currently in memory
    int countProcessesInMemory() const {
        int count = 0;
        for (const auto& block : blocks) {
            if (block.allocated && !block.owner.empty()) count++;
        }
        return count;
    }

    // Returns total external fragmentation in KB (sum of all free blocks)
    size_t getExternalFragmentation() const {
        size_t total = 0;
        for (const auto& block : blocks) {
            if (!block.allocated) total += block.size;
        }
        return total;
    }

    // ASCII printout of memory (returns as string)
    std::string asciiMemoryMap() const {
        std::ostringstream oss;
        size_t mem_end = blocks.empty() ? 0 : blocks.back().start + blocks.back().size;
        oss << "----end----- = " << mem_end << "\n";
        // Print only allocated blocks, from high to low address
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            const auto& block = *it;
            if (block.allocated && !block.owner.empty()) {
                size_t upper = block.start + block.size;
                size_t lower = block.start;
                oss << upper << "\n";
                oss << block.owner << "\n";
                oss << lower << "\n";
            }
        }
        oss << "----start----- = 0\n";
        // Print external fragmentation in bytes
        oss << "External fragmentation: " << getExternalFragmentation() << " bytes\n";
        return oss.str();
    }
public:
    MemoryManager(size_t totalSize) {
        blocks.push_back({0, totalSize, false, ""});
    }

    // First-fit allocation
    // Returns start address if successful, or -1 if failed
    int allocate(size_t size, const std::string& owner) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (!blocks[i].allocated && blocks[i].size >= size) {
                if (blocks[i].size > size) {
                    // Split block
                    MemoryBlock newBlock = {blocks[i].start + size, blocks[i].size - size, false, ""};
                    blocks[i].size = size;
                    blocks.insert(blocks.begin() + i + 1, newBlock);
                }
                blocks[i].allocated = true;
                blocks[i].owner = owner;
                return static_cast<int>(blocks[i].start);
            }
        }
        return -1;
    }

    // Free memory by owner (process name)
    void free(const std::string& owner) {
        for (auto& block : blocks) {
            if (block.allocated && block.owner == owner) {
                block.allocated = false;
                block.owner = "";
            }
        }
        // Merge adjacent free blocks
        mergeFreeBlocks();
    }

    void mergeFreeBlocks() {
        for (size_t i = 0; i + 1 < blocks.size(); ) {
            if (!blocks[i].allocated && !blocks[i+1].allocated) {
                blocks[i].size += blocks[i+1].size;
                blocks.erase(blocks.begin() + i + 1);
            } else {
                ++i;
            }
        }
    }

    void printMemory() const {
        std::cout << "--- Memory Blocks ---\n";
        for (const auto& block : blocks) {
            std::cout << "[" << block.start << ", " << (block.start + block.size - 1) << "] Size: " << block.size
                      << (block.allocated ? (" Allocated to: " + block.owner) : " Free") << "\n";
        }
    }

private:
    std::vector<MemoryBlock> blocks;
};

// Struct for process
struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
    std::string startTimestamp;
    std::string finishTimestamp;
    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    int coreAssigned = -1;
    int sleepRemaining = 0;
    size_t memoryRequired = 0;
    int memoryAddress = -1; // start address in memory manager
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
    size_t maxOverallMem = 1024;
    size_t memPerFrame = 0;
    size_t memPerProc = 64;

};

void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out = std::cout, int nestLevel = 0);
std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel);

class Scheduler {
    // Helper: produce memory report file
    void writeMemoryReport(int quantumCycle) {
        std::ostringstream fname;
        fname << "memory_stamp_" << quantumCycle << ".txt";
        std::ofstream out(fname.str());
        if (!out.is_open()) return;
        out << "Timestamp: (" << getCurrentTimestamp() << ")\n";
        out << "Number of processes in memory: " << memoryManager->countProcessesInMemory() << "\n";
        out << "Total external fragmentation in KB: " << memoryManager->getExternalFragmentation() << "\n";
        out << memoryManager->asciiMemoryMap();
        out.close();
    }
public:
    // Public method to free memory for a process by name
    void freeProcessMemory(const std::string& name) {
        memoryManager->free(name);
    }
private:
    std::vector<Process> processList;
    std::vector<int> finishedProcesses;
    std::vector<std::string> coreToProcess;
    std::mutex processMutex;
    bool schedulerRunning = false;
    std::thread schedulerMain;
    std::map<std::string, ScreenSession>* screenSessions = nullptr;
    std::unique_ptr<MemoryManager> memoryManager;
    size_t memPerProc;
    size_t memPerFrame;
    size_t maxOverallMem;
    std::deque<Process> pendingQueue; // for processes waiting for memory

    void cpuWorker(int coreID) {
        while (true) {
            processMutex.lock();
            int procIndex = -1;
            for (size_t i = 0; i < processList.size(); i++) {
                if (!processList[i].finished && processList[i].executedCommands < processList[i].totalCommands && processList[i].coreAssigned == -1) {
                    procIndex = i;
                    processList[i].coreAssigned = coreID - 1;
                    coreToProcess[coreID - 1] = processList[i].name;
                    break;
                }
            }
            processMutex.unlock();

            if (procIndex == -1) {
                if (!schedulerRunning) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
                continue;
            }

            std::string procName;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                Process& proc = processList[procIndex];

                if (proc.sleepRemaining > 0) {
                    proc.sleepRemaining--;
                    proc.coreAssigned = -1;
                    continue; 
                }

                std::ostringstream ss;
                ss << "(" << getCurrentTimestamp() << ") Core:" << (coreID - 1) << " ";
                std::string logLine;
                if (proc.executedCommands < proc.instructions.size()) {
                    Instruction& instr = proc.instructions[proc.executedCommands];
                    executeInstruction(proc, instr, ss);  
                    logLine = ss.str();
                }

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

                if (proc.executedCommands >= proc.totalCommands) {
                    proc.finished = true;
                    auto it = screenSessions->find(proc.name);
                    if (it != screenSessions->end()) {
                        it->second.currentLine = proc.executedCommands;
                    }
                    proc.finishTimestamp = getCurrentTimestamp();
                    finishedProcesses.push_back(procIndex);
                    proc.coreAssigned = -1;
                    coreToProcess[coreID - 1] = ""; 
                }
                else {
                    proc.coreAssigned = -1; 
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
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
                        processList[currentIndex].executedCommands < processList[currentIndex].totalCommands &&
                        processList[currentIndex].coreAssigned == -1) {
                        procIndex = currentIndex;
                        processList[currentIndex].coreAssigned = coreID - 1;
                        coreToProcess[coreID - 1] = processList[currentIndex].name;
                        break;
                    }

                    currentIndex = (currentIndex + 1) % procCount;
                } while (currentIndex != startIndex);
            }

            if (procIndex == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
                continue;
            }

            for (int i = 0; i < (int)quantumCycles; ++i) {
                std::lock_guard<std::mutex> lock(processMutex);
                if (procIndex >= processList.size()) break;

                Process& proc = processList[procIndex];

                if (proc.sleepRemaining > 0) {
                    proc.sleepRemaining--;
                    proc.coreAssigned = -1;
                    break; 
                }

                if (proc.finished || proc.executedCommands >= proc.totalCommands)
                    break;

                std::ostringstream ss;
                ss << "(" << getCurrentTimestamp() << ") Core:" << (coreID - 1) << " ";
                std::string logLine;
                if (proc.executedCommands < proc.instructions.size()) {
                    Instruction& instr = proc.instructions[proc.executedCommands];
                    executeInstruction(proc, instr, ss);
                    logLine = ss.str();
                }

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
                    proc.coreAssigned = -1;
                    coreToProcess[coreID - 1] = "";
                    break;
                }
                else {
                    proc.coreAssigned = -1;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
            }

            currentIndex = (currentIndex + 1) % processList.size();
        }
    }

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
            std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
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
        coreToProcess.resize(config.numCores, "");
        memoryManager = std::make_unique<MemoryManager>(config.maxOverallMem);
        memPerProc = config.memPerProc;
        memPerFrame = config.memPerFrame;
        maxOverallMem = config.maxOverallMem;
    }

    // Add process with memory allocation
    // If suppressError is true, do not print error message
    bool addProcess(Process& process, bool suppressError = false) {
        process.memoryRequired = memPerProc;
        int addr = memoryManager->allocate(process.memoryRequired, process.name);
        if (addr == -1) {
            if (!suppressError) {
                std::cout << "Failed to allocate memory for process '" << process.name << "' (" << process.memoryRequired << " units).\n";
            }
            return false;
        }
        process.memoryAddress = addr;
        processList.push_back(process);
        return true;
    }
    const std::vector<Process>& getProcessList() const {
        return processList;
    }
    const std::vector<int>& getFinishedProcesses() const {
        return finishedProcesses;
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
        int quantumCounter = 0;
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

        // Periodic memory report thread
        std::atomic<bool> reportThreadRunning(true);
        std::thread reportThread([this, &reportThreadRunning, &quantumCounter]() {
            while (reportThreadRunning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(quantumCycles));
                ++quantumCounter;
                writeMemoryReport(quantumCounter);
            }
        });
        // Process generator
        std::thread processCreator([this]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(minIns, maxIns);
            int processCounter = 1;

            while (schedulerRunning) {
                if (!schedulerRunning) break;
                Process proc;
                proc.name = "process" + std::to_string(processCounter++);
                proc.totalCommands = dist(gen);
                proc.executedCommands = 0;
                proc.startTimestamp = getCurrentTimestamp();
                proc.finished = false;
                proc.coreAssigned = -1;
                proc.instructions = generateRandomInstructions(proc.name, proc.totalCommands, 0);

                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    pendingQueue.push_back(proc);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(batchProcFreq));
            }
        });

        // Pending queue memory allocation loop
        std::thread pendingAllocator([this]() {
            while (schedulerRunning) {
                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    if (!pendingQueue.empty()) {
                        size_t n = pendingQueue.size();
                        for (size_t i = 0; i < n; ++i) {
                            Process proc = pendingQueue.front();
                            pendingQueue.pop_front();
                            bool added = addProcess(proc, true); // suppress error message on retry
                            if (added) {
                                if (screenSessions) {
                                    (*screenSessions)[proc.name] = {
                                        proc.name, 1, proc.totalCommands, proc.startTimestamp
                                    };
                                }
                            } else {
                                pendingQueue.push_back(proc); // move to back
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });

        // Check for processes if done
        std::thread schedThread(&Scheduler::scheduler, this);

        // Wait for processes to end
        while (schedulerRunning && soloProcessCount > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        reportThreadRunning = false;
        if (reportThread.joinable()) reportThread.join();
        if (processCreator.joinable()) processCreator.join();
        if (pendingAllocator.joinable()) pendingAllocator.join();
        if (schedThread.joinable()) schedThread.join();
        for (auto& t : cpuThreads) t.join();
    }

    void generateLog(const std::string& filename) {
        std::ofstream reportFile(filename);
        if (!reportFile.is_open()) {
            std::cerr << "Failed to open " << filename << " for writing.\n";
            return;
        }

        reportFile << std::left;
        int nameWidth = 12;

        int totalCores = static_cast<int>(coreToProcess.size());
        int coresUsed = 0;
        for (const auto& procName : coreToProcess) {
            if (!procName.empty()) {
                coresUsed++;
            }
        }
        int coresAvailable = totalCores - coresUsed;
        double cpuUtil = (static_cast<double>(coresUsed) / totalCores) * 100.0;
        reportFile << "---------------------------------------------\n";
        reportFile << "CPU Status:\n";
        reportFile << "Total Cores      : " << totalCores << "\n";
        reportFile << "Cores Used       : " << coresUsed << "\n";
        reportFile << "Cores Available  : " << coresAvailable << "\n";
        reportFile << "CPU Utilization  : " << static_cast<int>(cpuUtil) << "%\n\n";
        reportFile << "---------------------------------------------\n";
        reportFile << "Running processes:\n";
        for (int core = 0; core < coreToProcess.size(); ++core) {
            const std::string& pname = coreToProcess[core];
            if (pname.empty()) continue;

            for (const auto& proc : processList) {
                if (proc.name == pname) {
                    reportFile << std::setw(nameWidth) << proc.name << "  ";
                    reportFile << "(Started: " << proc.startTimestamp << ")  ";
                    reportFile << "Core: " << core << "  ";
                    reportFile << proc.executedCommands << " / " << proc.totalCommands << "\n";
                    break;
                }
            }
        }
        reportFile << "\nFinished processes:\n";
        for (size_t i = 0; i < processList.size(); i++) {
            if (processList[i].finished) {
                reportFile << std::setw(nameWidth) << processList[i].name << "  ";
                reportFile << "(" << processList[i].finishTimestamp << ")  ";
                reportFile << "Finished  ";
                reportFile << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
            }
        }
        reportFile << "---------------------------------------------\n";
    }

    void shutdown() {
        schedulerRunning = false;
        if (schedulerMain.joinable()) {
            schedulerMain.join();
        }
        // Free memory for all finished processes
        for (const auto& proc : processList) {
            if (proc.finished && proc.memoryAddress != -1) {
                memoryManager->free(proc.name);
            }
        }
    }
    void printMemory() const {
        memoryManager->printMemory();
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
        std::cout << "Max Overall Memory    : " << maxOverallMem << "\n";
        std::cout << "Memory Per Frame      : " << memPerFrame << "\n";
        std::cout << "Memory Per Process    : " << memPerProc << "\n";
        std::cout << "----------------------------------\n";
    }

    // show process status
    void showScreenLS() {
        processMutex.lock();
        std::cout << std::left; 
        int nameWidth = 12;

        int totalCores = static_cast<int>(coreToProcess.size());
        int coresUsed = 0;
        for (const auto& procName : coreToProcess) {
            if (!procName.empty()) {
                coresUsed++;
            }
        }
        int coresAvailable = totalCores - coresUsed;
        double cpuUtil = (static_cast<double>(coresUsed) / totalCores) * 100.0;
        std::cout << getProcessList().size();
        std::cout << "---------------------------------------------\n";
        std::cout << "CPU Status:\n";
        std::cout << "Total Cores      : " << totalCores << "\n";
        std::cout << "Cores Used       : " << coresUsed << "\n";
        std::cout << "Cores Available  : " << coresAvailable << "\n";
        std::cout << "CPU Utilization  : " << static_cast<int>(cpuUtil) << "%\n\n";
        std::cout << "---------------------------------------------\n";
        std::cout << "Running processes:\n";
        for (int core = 0; core < coreToProcess.size(); ++core) {
            const std::string& pname = coreToProcess[core];
            if (pname.empty()) continue;

            for (const auto& proc : processList) {
                if (proc.name == pname) {
                    std::cout << std::setw(nameWidth) << proc.name << "  ";
                    std::cout << "(Started: " << proc.startTimestamp << ")  ";
                    std::cout << "Core: " << core << "  ";
                    std::cout << proc.executedCommands << " / " << proc.totalCommands << "\n";
                    break;
                }
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
    std::mutex& getProcessMutex() { return processMutex; }
    std::vector<std::string>& getCoreToProcess() { return coreToProcess; }
};

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
}

// Loop for inside screen session
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

            for (const auto& proc : scheduler->getProcessList()) {
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
            else if (attribute == "max-overall-mem") {
                config->maxOverallMem = std::stoull(configVals);
            }
            else if (attribute == "mem-per-frame") {
                config->memPerFrame = std::stoull(configVals);
            }
            else if (attribute == "mem-per-proc") {
                config->memPerProc = std::stoull(configVals);
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

    //check if process already exists
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

    // create new process
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

    // Try to allocate memory and add process
    if (!scheduler->addProcess(proc)) {
        std::cout << "Process creation failed due to insufficient memory.\n";
        return;
    }
    screens[name] = { name, 1, proc.totalCommands, proc.startTimestamp };

    if (!scheduler->isRunning()) {
        auto& coreToProcess = scheduler->getCoreToProcess();
        int assignedCore = -1;

        // fcfs for -s processes
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

        std::thread soloWorker([scheduler, &screens, name, assignedCore]() {
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
                                // Free memory on finish
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
    std::cout << "Commands:\n"
        << "  initialize       - Initialize the processor configuration with \"config.txt\".\n"
        << "  screen -s <name> - Attach or create a screen session for a process.\n"
        << "  screen -r <name> - Resume an existing screen session if still running.\n"
        << "       process-smi - Show process info inside screen.\n"
        << "       exit        - Exit the screen session.\n"
        << "  screen -ls       - Show current CPU/process usage.\n"
        << "  scheduler-start  - Start dummy process generation.\n"
        << "  scheduler-stop   - Stop process generation and free memory.\n"
        << "  report-util      - Save CPU utilization report to file.\n"
        << "  clear            - Clear the screen.\n"
        << "  exit             - Exit the emulator.\n";
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

            if (nestLevel >= 3) break; // max 3 nested loops 

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
    std::unique_ptr<Scheduler> procScheduler;

    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        std::transform(inputCommand.begin(), inputCommand.end(), inputCommand.begin(), ::tolower);

        if (inputCommand.find("initialize") != std::string::npos) {
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
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            while (soloProcessCount > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            menuState = false;
            procScheduler.reset();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else if (!initialized) {
            std::cout << "Run the initialize command first before proceeding.\n";
        }
        else if (inputCommand.rfind("screen -s ", 0) == 0) {
            handleScreenS(inputCommand.substr(10), procScheduler.get(), screens);
        }
        else if (inputCommand.rfind("screen -r ", 0) == 0) {
            handleScreenR(inputCommand.substr(10), screens, procScheduler.get());
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
            if (procScheduler) {
                procScheduler->generateLog("csopesy-log.txt");
                procScheduler->printMemory();
            }
            else {
                std::cout << "Scheduler not initialized.\n";
            }
        }
        else if (inputCommand == "mem-status") {
            if (procScheduler) {
                procScheduler->printMemory();
            } else {
                std::cout << "Scheduler not initialized.\n";
            }
        }
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }
        inputCommand = "";
    }
    return 0;
}