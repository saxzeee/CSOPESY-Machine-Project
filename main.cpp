// CSOPESY Enhanced OS Emulator - Complete Implementation
// This is a complete rewrite with modern C++ practices and robust architecture

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <random>
#include <functional>

// Enhanced system-wide configuration
struct SystemConfig {
    int numCpu = 4;
    std::string scheduler = "fcfs";  // fcfs, rr, priority
    int quantumCycles = 5;
    int batchProcessFreq = 1;
    int minInstructions = 1000;
    int maxInstructions = 2000;
    int delayPerExec = 100;
    int maxOverallMem = 1024;
    int memPerFrame = 16;
    int memPerProcess = 64;
    
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            
            if (line.empty() || line[0] == '#') continue; // Skip comments and empty lines
            
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            try {
                if (key == "num-cpu") numCpu = std::stoi(value);
                else if (key == "scheduler") scheduler = value;
                else if (key == "quantum-cycles") quantumCycles = std::stoi(value);
                else if (key == "batch-process-freq") batchProcessFreq = std::stoi(value);
                else if (key == "min-ins") minInstructions = std::stoi(value);
                else if (key == "max-ins") maxInstructions = std::stoi(value);
                else if (key == "delay-per-exec") delayPerExec = std::stoi(value);
                else if (key == "max-overall-mem") maxOverallMem = std::stoi(value);
                else if (key == "mem-per-frame") memPerFrame = std::stoi(value);
                else if (key == "mem-per-proc") memPerProcess = std::stoi(value);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing config line: " << line << std::endl;
            }
        }
        
        return true;
    }
    
    void display() const {
        std::cout << "---- Scheduler Configuration ----" << std::endl;
        std::cout << "Number of CPU Cores   : " << numCpu << std::endl;
        std::cout << "Scheduling Algorithm  : " << scheduler << std::endl;
        std::cout << "Quantum Cycles        : " << quantumCycles << std::endl;
        std::cout << "Batch Process Freq    : " << batchProcessFreq << std::endl;
        std::cout << "Min Instructions      : " << minInstructions << std::endl;
        std::cout << "Max Instructions      : " << maxInstructions << std::endl;
        std::cout << "Delay per Execution   : " << delayPerExec << std::endl;
        std::cout << "Max Overall Memory    : " << maxOverallMem << std::endl;
        std::cout << "Memory Per Frame      : " << memPerFrame << std::endl;
        std::cout << "Memory Per Process    : " << memPerProcess << std::endl;
        std::cout << "----------------------------------" << std::endl;
    }
};

// Enhanced Process State Management
enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
};

// Utility functions
namespace Utils {
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%m/%d/%Y %I:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        ss << std::put_time(std::localtime(&time_t), " %p");
        
        return ss.str();
    }
    
    void clearScreen() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
    
    void setTextColor(int color) {
        std::cout << "\033[" << color << "m";
    }
    
    void resetTextColor() {
        std::cout << "\033[0m";
    }
    
    std::string formatDuration(std::chrono::milliseconds duration) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto ms = duration - seconds;
        
        return std::to_string(seconds.count()) + "." + 
               std::to_string(ms.count()) + "s";
    }
    
    int generateRandomInt(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(min, max);
        return dist(gen);
    }
}

// Comprehensive Process Control Block
class Process {
public:
    std::string pid;
    std::string name;
    ProcessState state;
    int priority;
    int arrivalTime;
    int burstTime;
    int remainingTime;
    int executedInstructions;
    int totalInstructions;
    int coreAssignment;
    int memoryAddress;
    int memorySize;
    std::string creationTimestamp;
    std::string completionTimestamp;
    std::vector<std::string> instructionHistory;
    std::queue<std::string> pendingInstructions;
    
    // Performance metrics
    int waitingTime = 0;
    int turnaroundTime = 0;
    int responseTime = -1;
    
    Process(const std::string& processName) 
        : name(processName), state(ProcessState::NEW), priority(0), 
          coreAssignment(-1), memoryAddress(-1), memorySize(0),
          executedInstructions(0), totalInstructions(0) {
        
        // Generate unique PID
        static std::atomic<int> pidCounter{1};
        pid = "p" + std::string(3 - std::to_string(pidCounter.load()).length(), '0') + 
              std::to_string(pidCounter.fetch_add(1));
        
        creationTimestamp = Utils::getCurrentTimestamp();
        arrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    void generateInstructions(int count) {
        totalInstructions = count;
        remainingTime = count;
        burstTime = count;
        
        // Generate diverse instruction types
        std::vector<std::string> instructionTypes = {
            "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV", 
            "CMP", "JMP", "CALL", "RET", "PUSH", "POP"
        };
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> typeDist(0, instructionTypes.size() - 1);
        std::uniform_int_distribution<> valueDist(1, 1000);
        
        for (int i = 0; i < count; ++i) {
            std::string instruction = instructionTypes[typeDist(gen)] + " " + 
                                    std::to_string(valueDist(gen));
            pendingInstructions.push(instruction);
        }
    }
    
    std::string executeNextInstruction() {
        if (pendingInstructions.empty()) {
            return "";
        }
        
        std::string instruction = pendingInstructions.front();
        pendingInstructions.pop();
        
        // Simulate instruction execution
        std::string timestamp = Utils::getCurrentTimestamp();
        std::string logEntry = "(" + timestamp + ") " + instruction;
        
        instructionHistory.push_back(logEntry);
        executedInstructions++;
        remainingTime--;
        
        // Set response time on first execution
        if (responseTime == -1) {
            responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count() - arrivalTime;
        }
        
        return logEntry;
    }
    
    bool isComplete() const {
        return pendingInstructions.empty() && executedInstructions >= totalInstructions;
    }
    
    void updateMetrics() {
        int currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        if (state == ProcessState::TERMINATED) {
            turnaroundTime = currentTime - arrivalTime;
            waitingTime = turnaroundTime - burstTime;
            completionTimestamp = Utils::getCurrentTimestamp();
        }
    }
    
    std::string getStateString() const {
        switch (state) {
            case ProcessState::NEW: return "NEW";
            case ProcessState::READY: return "READY";
            case ProcessState::RUNNING: return "RUNNING";
            case ProcessState::WAITING: return "WAITING";
            case ProcessState::TERMINATED: return "TERMINATED";
            default: return "UNKNOWN";
        }
    }
};

// Advanced Memory Management with multiple allocation strategies
class MemoryManager {
private:
    int totalMemory;
    int frameSize;
    std::vector<bool> memoryMap;
    std::map<std::string, std::pair<int, int>> allocatedProcesses; // pid -> (address, size)
    mutable std::mutex memoryMutex;
    
public:
    enum class AllocationStrategy {
        FIRST_FIT,
        BEST_FIT,
        WORST_FIT
    };
    
    AllocationStrategy strategy = AllocationStrategy::FIRST_FIT;
    
    MemoryManager(int totalMem, int frameSize) 
        : totalMemory(totalMem), frameSize(frameSize) {
        int numFrames = totalMemory / frameSize;
        memoryMap.resize(numFrames, false); // false = free, true = allocated
    }
    
    bool allocate(const std::string& pid, int size) {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        int framesNeeded = (size + frameSize - 1) / frameSize; // Ceiling division
        int totalFrames = memoryMap.size();
        
        int startFrame = -1;
        
        // First fit strategy (simplified for this example)
        int consecutiveFree = 0;
        for (int i = 0; i < totalFrames; ++i) {
            if (!memoryMap[i]) {
                if (consecutiveFree == 0) startFrame = i;
                consecutiveFree++;
                if (consecutiveFree >= framesNeeded) break;
            } else {
                consecutiveFree = 0;
                startFrame = -1;
            }
        }
        
        if (startFrame == -1) {
            return false; // No suitable block found
        }
        
        // Allocate the frames
        for (int i = startFrame; i < startFrame + framesNeeded; ++i) {
            memoryMap[i] = true;
        }
        
        allocatedProcesses[pid] = {startFrame * frameSize, framesNeeded * frameSize};
        return true;
    }
    
    bool deallocate(const std::string& pid) {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        auto it = allocatedProcesses.find(pid);
        if (it == allocatedProcesses.end()) {
            return false; // Process not found
        }
        
        int startAddress = it->second.first;
        int size = it->second.second;
        int startFrame = startAddress / frameSize;
        int framesCount = (size + frameSize - 1) / frameSize;
        
        // Free the frames
        for (int i = startFrame; i < startFrame + framesCount; ++i) {
            memoryMap[i] = false;
        }
        
        allocatedProcesses.erase(it);
        return true;
    }
    
    void displayMemoryMap() const {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        std::cout << "\n=== Memory Map ===" << std::endl;
        std::cout << "Total Memory: " << totalMemory << " KB" << std::endl;
        std::cout << "Frame Size: " << frameSize << " KB" << std::endl;
        std::cout << "Available: " << getAvailableMemory() << " KB" << std::endl;
        
        std::cout << "\nAllocated Processes:" << std::endl;
        for (const auto& [pid, allocation] : allocatedProcesses) {
            std::cout << "  " << pid << ": Address " << allocation.first 
                      << ", Size " << allocation.second << " KB" << std::endl;
        }
        
        // Visual representation
        std::cout << "\nMemory Layout: ";
        const int maxDisplay = 50;
        int step = std::max(1, static_cast<int>(memoryMap.size()) / maxDisplay);
        
        for (size_t i = 0; i < memoryMap.size(); i += step) {
            std::cout << (memoryMap[i] ? "█" : "░");
        }
        std::cout << std::endl;
        std::cout << "░ = Free, █ = Allocated" << std::endl;
    }
    
    int getAvailableMemory() const {
        int freeFrames = 0;
        for (bool allocated : memoryMap) {
            if (!allocated) freeFrames++;
        }
        return freeFrames * frameSize;
    }
};

// Forward declaration
class Scheduler;

// High-performance Scheduler with pluggable algorithms
class Scheduler {
private:
    std::unique_ptr<SystemConfig> config;
    std::unique_ptr<MemoryManager> memoryManager;
    
    // Process management
    std::vector<std::shared_ptr<Process>> allProcesses;
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> runningProcesses; // One per core
    std::vector<std::shared_ptr<Process>> terminatedProcesses;
    
    // Threading and synchronization
    std::vector<std::thread> coreWorkers;
    std::atomic<bool> isRunning{false};
    std::atomic<bool> shouldStop{false};
    mutable std::mutex processMutex;
    std::condition_variable processCV;
    
    // Performance tracking
    std::atomic<int> processCounter{1};
    std::chrono::high_resolution_clock::time_point systemStartTime;
    
    void coreWorkerThread(int coreId) {
        while (!shouldStop.load()) {
            std::shared_ptr<Process> currentProcess = nullptr;
            
            {
                std::unique_lock<std::mutex> lock(processMutex);
                
                // Check if there's already a process assigned to this core
                currentProcess = runningProcesses[coreId];
                
                // If no process assigned, try to get one from ready queue
                if (!currentProcess && !readyQueue.empty()) {
                    currentProcess = readyQueue.front();
                    readyQueue.pop();
                    
                    runningProcesses[coreId] = currentProcess;
                    currentProcess->state = ProcessState::RUNNING;
                    currentProcess->coreAssignment = coreId;
                }
            }
            
            if (currentProcess) {
                // Execute one instruction
                std::string logEntry = currentProcess->executeNextInstruction();
                
                // Check if process is complete
                if (currentProcess->isComplete()) {
                    handleProcessCompletion(currentProcess);
                    
                    std::lock_guard<std::mutex> lock(processMutex);
                    runningProcesses[coreId] = nullptr;
                }
                // Check for Round Robin preemption
                else if (config->scheduler == "rr") {
                    static std::map<int, int> coreQuantumCounters;
                    coreQuantumCounters[coreId]++;
                    
                    if (coreQuantumCounters[coreId] >= config->quantumCycles) {
                        coreQuantumCounters[coreId] = 0;
                        
                        std::lock_guard<std::mutex> lock(processMutex);
                        currentProcess->state = ProcessState::READY;
                        currentProcess->coreAssignment = -1;
                        readyQueue.push(currentProcess);
                        runningProcesses[coreId] = nullptr;
                        processCV.notify_one();
                    }
                }
                
                // Execution delay
                std::this_thread::sleep_for(std::chrono::milliseconds(config->delayPerExec));
            } else {
                // No process to run, wait briefly
                std::unique_lock<std::mutex> lock(processMutex);
                processCV.wait_for(lock, std::chrono::milliseconds(50));
            }
        }
    }
    
    void processCreatorThread() {
        while (!shouldStop.load()) {
            // Create a new process
            createProcess();
            
            // Wait for the specified frequency
            std::this_thread::sleep_for(std::chrono::seconds(config->batchProcessFreq));
        }
    }
    
    void handleProcessCompletion(std::shared_ptr<Process> process) {
        process->state = ProcessState::TERMINATED;
        process->updateMetrics();
        process->coreAssignment = -1;
        
        // Free memory
        memoryManager->deallocate(process->pid);
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            terminatedProcesses.push_back(process);
        }
        
        // Process completed silently during scheduler operation
    }
    
public:
    Scheduler(std::unique_ptr<SystemConfig> cfg) 
        : config(std::move(cfg)) {
        
        memoryManager = std::make_unique<MemoryManager>(
            config->maxOverallMem, config->memPerFrame);
        
        runningProcesses.resize(config->numCpu, nullptr);
        systemStartTime = std::chrono::high_resolution_clock::now();
    }
    
    ~Scheduler() {
        stop();
    }
    
    bool start() {
        if (isRunning.load()) {
            std::cout << "Scheduler is already running." << std::endl;
            return false;
        }
        
        shouldStop.store(false);
        isRunning.store(true);
        
        // Start core worker threads
        coreWorkers.clear();
        for (int i = 0; i < config->numCpu; ++i) {
            coreWorkers.emplace_back(&Scheduler::coreWorkerThread, this, i);
        }
        
        // Start process creator thread
        coreWorkers.emplace_back(&Scheduler::processCreatorThread, this);
        
        std::cout << "Scheduler started with " << config->numCpu << " CPU cores." << std::endl;
        return true;
    }
    
    void stop() {
        if (!isRunning.load()) {
            return;
        }
        
        shouldStop.store(true);
        isRunning.store(false);
        
        // Notify all waiting threads
        processCV.notify_all();
        
        // Wait for all threads to complete
        for (auto& worker : coreWorkers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        coreWorkers.clear();
        std::cout << "Scheduler stopped successfully." << std::endl;
    }
    
    bool createProcess(const std::string& name = "") {
        // Generate process name if not provided
        std::string processName = name;
        if (processName.empty()) {
            processName = "process" + std::to_string(processCounter.fetch_add(1));
        }
        
        // Create new process
        auto process = std::make_shared<Process>(processName);
        
        // Generate random number of instructions
        int instructionCount = Utils::generateRandomInt(
            config->minInstructions, config->maxInstructions);
        process->generateInstructions(instructionCount);
        
        // Try to allocate memory
        if (!memoryManager->allocate(process->pid, config->memPerProcess)) {
            std::cout << "Failed to create process '" << processName 
                      << "': Insufficient memory." << std::endl;
            return false;
        }
        
        process->memorySize = config->memPerProcess;
        process->state = ProcessState::READY;
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            allProcesses.push_back(process);
            readyQueue.push(process);
        }
        
        processCV.notify_one();
        
        // Process created silently during scheduler operation
        return true;
    }
    
    void displaySystemStatus() const {
        std::lock_guard<std::mutex> lock(processMutex);
        
        // Calculate CPU utilization
        int busyCores = 0;
        for (const auto& process : runningProcesses) {
            if (process != nullptr) busyCores++;
        }
        
        int totalCores = config->numCpu;
        int coresAvailable = totalCores - busyCores;
        double cpuUtilization = (static_cast<double>(busyCores) / totalCores) * 100.0;
        
        std::cout << "---------------------------------------------" << std::endl;
        std::cout << "CPU Status:" << std::endl;
        std::cout << "Total Cores      : " << totalCores << std::endl;
        std::cout << "Cores Used       : " << busyCores << std::endl;
        std::cout << "Cores Available  : " << coresAvailable << std::endl;
        std::cout << "CPU Utilization  : " << static_cast<int>(cpuUtilization) << "%" << std::endl << std::endl;
        std::cout << "---------------------------------------------" << std::endl;
        std::cout << "Running processes:" << std::endl;
    }
    
    void displayProcesses() const {
        std::lock_guard<std::mutex> lock(processMutex);
        
        // Display running processes
        for (size_t i = 0; i < runningProcesses.size(); ++i) {
            if (runningProcesses[i]) {
                auto& p = runningProcesses[i];
                std::cout << std::left << std::setw(12) << p->name << "  ";
                std::cout << "(Started: " << p->creationTimestamp << ")  ";
                std::cout << "Core: " << i << "  ";
                std::cout << p->executedInstructions << " / " << p->totalInstructions << std::endl;
            }
        }
        
        std::cout << std::endl << "Finished processes:" << std::endl;
        for (const auto& p : terminatedProcesses) {
            std::cout << std::left << std::setw(12) << p->name << "  ";
            std::cout << "(" << p->completionTimestamp << ")  ";
            std::cout << "Finished  ";
            std::cout << p->executedInstructions << " / " << p->totalInstructions << std::endl;
        }
        std::cout << "---------------------------------------------" << std::endl;
    }
    
    void generateReport(const std::string& filename) const {
        // Try to acquire lock with simple try_lock to prevent freezing
        std::unique_lock<std::mutex> lock(processMutex, std::defer_lock);
        if (!lock.try_lock()) {
            std::cout << "System busy, please try generating report again." << std::endl;
            return;
        }
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot create report file: " << filename << std::endl;
            return;
        }
        
        file << "CSOPESY Enhanced Process Scheduler Report" << std::endl;
        file << "Generated: " << Utils::getCurrentTimestamp() << std::endl;
        file << std::string(50, '=') << std::endl;
        
        // System configuration
        file << "\nSystem Configuration:" << std::endl;
        file << "CPU Cores: " << config->numCpu << std::endl;
        file << "Scheduler Algorithm: " << config->scheduler << std::endl;
        file << "Quantum Cycles: " << config->quantumCycles << std::endl;
        file << "Memory: " << config->maxOverallMem << " KB" << std::endl;
        
        // Current status
        int busyCores = 0;
        for (const auto& process : runningProcesses) {
            if (process != nullptr) busyCores++;
        }
        
        file << "\nCurrent System Status:" << std::endl;
        file << "CPU Utilization: " << std::fixed << std::setprecision(1) 
             << (static_cast<double>(busyCores) / config->numCpu) * 100.0 << "%" << std::endl;
        file << "Active Processes: " << busyCores << std::endl;
        file << "Ready Queue: " << readyQueue.size() << std::endl;
        file << "Completed Processes: " << terminatedProcesses.size() << std::endl;
        
        file.close();
        std::cout << "Report generated: " << filename << std::endl;
    }
    
    std::shared_ptr<Process> findProcess(const std::string& name) const {
        std::lock_guard<std::mutex> lock(processMutex);
        
        for (const auto& process : allProcesses) {
            if (process->name == name || process->pid == name) {
                return process;
            }
        }
        return nullptr;
    }
    
    bool isSystemRunning() const { return isRunning.load(); }
    
    void displayMemoryStatus() const {
        memoryManager->displayMemoryMap();
    }
};

// Enhanced Command Processing with better error handling
class CommandProcessor {
private:
    std::unique_ptr<Scheduler> scheduler;
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> commands;
    bool initialized = false;
    
    std::vector<std::string> parseCommand(const std::string& input) {
        std::vector<std::string> tokens;
        std::istringstream iss(input);
        std::string token;
        
        while (iss >> token) {
            tokens.push_back(token);
        }
        
        return tokens;
    }
    
public:
    CommandProcessor() {
        // Initialize command handlers
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
            
            if (scheduler->start()) {
                Utils::setTextColor(32); // Green
                std::cout << "Scheduler started successfully!" << std::endl;
                Utils::resetTextColor();
            }
        };
        
        commands["scheduler-stop"] = [this](const std::vector<std::string>& args) {
            if (!initialized) {
                std::cout << "Please initialize the system first." << std::endl;
                return;
            }
            
            scheduler->stop();
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
                
                // Create the process if it doesn't exist
                auto process = scheduler->findProcess(processName);
                if (!process) {
                    scheduler->createProcess(processName);
                    process = scheduler->findProcess(processName);
                }
                
                if (process) {
                    // Simple screen session simulation  
                    Utils::clearScreen();
                    Utils::setTextColor(36); // Cyan
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
                            if (!process->instructionHistory.empty()) {
                                std::cout << "\n--- Process Logs ---" << std::endl;
                                for (const auto& log : process->instructionHistory) {
                                    std::cout << log << std::endl;
                                }
                            } else {
                                std::cout << "No logs found for this process." << std::endl;
                            }
                            
                            if (process->state == ProcessState::TERMINATED) {
                                std::cout << "Finished!" << std::endl;
                            }
                        } else {
                            std::cout << "Available commands: process-smi, exit" << std::endl;
                        }
                    }
                }
            }
        };
        
        commands["report-util"] = [this](const std::vector<std::string>& args) {
            if (!initialized) {
                std::cout << "Please initialize the system first." << std::endl;
                return;
            }
            
            std::string filename = "csopesy-log.txt";
            scheduler->generateReport(filename);
        };
        
        commands["help"] = [this](const std::vector<std::string>& args) {
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
        };
        
        commands["clear"] = [this](const std::vector<std::string>& args) {
            displayHeader();
        };
    }
    
    void run() {
        std::string input;
        
        while (true) {
            std::cout << "\nEnter a command: ";
            
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            // Convert to lowercase for command matching
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
            
            // Handle special compound commands
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
                    Utils::setTextColor(31); // Red
                    std::cerr << "Error executing command: " << e.what() << std::endl;
                    Utils::resetTextColor();
                }
            } else {
                Utils::setTextColor(33); // Yellow
                std::cout << "Command not recognized. Type 'help' for available commands." << std::endl;
                Utils::resetTextColor();
            }
        }
    }
    
    void displayHeader() {
        Utils::clearScreen();
        Utils::setTextColor(34); // Blue
        
        std::cout << R"(
     _/_/_/    _/_/_/    _/_/    _/_/_/    _/_/_/_/    _/_/_/  _/      _/  
  _/        _/        _/    _/  _/    _/  _/        _/          _/  _/     
 _/          _/_/    _/    _/  _/_/_/    _/_/_/      _/_/        _/        
_/              _/  _/    _/  _/        _/              _/      _/         
 _/_/_/  _/_/_/      _/_/    _/        _/_/_/_/  _/_/_/        _/                                                                               
)" << std::endl;
        
        Utils::setTextColor(32); // Green
        std::cout << "Welcome to CSOPESY OS Emulator!" << std::endl;
        std::cout << "Type 'exit' to quit, 'clear' to clear the screen." << std::endl;
        Utils::resetTextColor();
    }
};

// Main function
int main() {
    try {
        CommandProcessor processor;
        processor.displayHeader();
        processor.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
