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

class Process;
class Scheduler;
class SystemConfig;
class CommandProcessor;

struct SystemConfig {
    int numCpu = 4;
    std::string scheduler = "fcfs";  
    int quantumCycles = 5;
    int batchProcessFreq = 1;
    int minInstructions = 1000;
    int maxInstructions = 2000;
    int delayPerExec = 100;
    
    bool loadFromFile(const std::string& filename);
    void display() const;
};

enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
};

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
    std::string creationTimestamp;
    std::string completionTimestamp;
    std::vector<std::string> instructionHistory;
    std::queue<std::string> pendingInstructions;
    
    int waitingTime = 0;
    int turnaroundTime = 0;
    int responseTime = -1;
    
    Process(const std::string& processName);
    void generateInstructions(int count);
    std::string executeNextInstruction();
    bool isComplete() const;
    void updateMetrics();
    std::string getStateString() const;
};

class Scheduler {
private:
    std::unique_ptr<SystemConfig> config;
    
    std::vector<std::shared_ptr<Process>> allProcesses;
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> runningProcesses; 
    std::queue<std::shared_ptr<Process>> waitingQueue;
    std::vector<std::shared_ptr<Process>> terminatedProcesses;
    
    std::vector<std::thread> coreWorkers;
    std::atomic<bool> isRunning{false};
    std::atomic<bool> shouldStop{false};
    mutable std::mutex processMutex;
    std::condition_variable processCV;
    
    std::atomic<int> processCounter{1};
    std::chrono::high_resolution_clock::time_point systemStartTime;
    
    void coreWorkerThread(int coreId);
    void processCreatorThread();
    std::shared_ptr<Process> selectNextProcess();
    void scheduleProcess(std::shared_ptr<Process> process, int coreId);
    void handleProcessCompletion(std::shared_ptr<Process> process);
    
public:
    Scheduler(std::unique_ptr<SystemConfig> config);
    ~Scheduler();
    
    bool start();
    void stop();
    bool createProcess(const std::string& name = "");
    void displayProcesses() const;
    void displaySystemStatus() const;
    void generateReport(const std::string& filename) const;
    std::shared_ptr<Process> findProcess(const std::string& name) const;
    bool isSystemRunning() const { return isRunning.load(); }
};

class CommandProcessor {
private:
    std::unique_ptr<Scheduler> scheduler;
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> commands;
    bool initialized = false;
    
    void initializeCommands();
    std::vector<std::string> parseCommand(const std::string& input);
    
    void handleInitialize(const std::vector<std::string>& args);
    void handleScreenS(const std::vector<std::string>& args);
    void handleScreenR(const std::vector<std::string>& args);
    void handleScreenLS(const std::vector<std::string>& args);
    void handleSchedulerStart(const std::vector<std::string>& args);
    void handleSchedulerStop(const std::vector<std::string>& args);
    void handleReportUtil(const std::vector<std::string>& args);
    void handleHelp(const std::vector<std::string>& args);
    void handleClear(const std::vector<std::string>& args);
    void handleProcessSMI(const std::vector<std::string>& args);
    
public:
    CommandProcessor();
    void run();
    void displayHeader();
};

namespace Utils {
    std::string getCurrentTimestamp();
    void clearScreen();
    void setTextColor(int color);
    void resetTextColor();
    std::string formatDuration(std::chrono::milliseconds duration);
    int generateRandomInt(int min, int max);
}

bool SystemConfig::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        
        if (line.empty() || line[0] == '#') continue; 
        
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
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config line: " << line << std::endl;
        }
    }
    
    return true;
}

void SystemConfig::display() const {
    std::cout << "=== System Configuration ===" << std::endl;
    std::cout << "CPU Cores: " << numCpu << std::endl;
    std::cout << "Scheduler: " << scheduler << std::endl;
    std::cout << "Quantum Cycles: " << quantumCycles << std::endl;
    std::cout << "Batch Process Frequency: " << batchProcessFreq << "ms" << std::endl;
    std::cout << "Instructions Range: " << minInstructions << "-" << maxInstructions << std::endl;
    std::cout << "Execution Delay: " << delayPerExec << "ms" << std::endl;
    std::cout << "=============================" << std::endl;
}

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
