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
#include <set>

struct SystemConfig {
    int numCpu = 4;
    std::string scheduler = "fcfs";
    int quantumCycles = 5;
    int batchProcessFreq = 1;
    int minInstructions = 1000;
    int maxInstructions = 2000;
    int delayPerExec = 100;
    
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);
            
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find(' ');
            if (pos == std::string::npos) {
                pos = line.find('=');
                if (pos == std::string::npos) continue;
            }
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            start = value.find_first_not_of(" \t");
            if (start != std::string::npos) {
                end = value.find_last_not_of(" \t");
                value = value.substr(start, end - start + 1);
            }
            
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
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
    
    void display() const {
        std::cout << "---- Scheduler Configuration ----" << std::endl;
        std::cout << "Number of CPU Cores   : " << numCpu << std::endl;
        std::cout << "Scheduling Algorithm  : " << scheduler << std::endl;
        std::cout << "Quantum Cycles        : " << quantumCycles << std::endl;
        std::cout << "Batch Process Freq    : " << batchProcessFreq << std::endl;
        std::cout << "Min Instructions      : " << minInstructions << std::endl;
        std::cout << "Max Instructions      : " << maxInstructions << std::endl;
        std::cout << "Delay per Execution   : " << delayPerExec << std::endl;
        std::cout << "----------------------------------" << std::endl;
    }
};

enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
};

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
    
    Process(const std::string& processName) 
        : name(processName), state(ProcessState::NEW), priority(0), 
          coreAssignment(-1),
          executedInstructions(0), totalInstructions(0) {
        
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
        
        std::random_device rd;
        std::mt19937 gen(rd());
        
        std::vector<std::string> varNames = {"x", "y", "z", "counter", "sum", "temp", "result", "value"};
        
        std::uniform_int_distribution<> varDist(0, varNames.size() - 1);
        std::uniform_int_distribution<> valueDist(1, 100);
        std::uniform_int_distribution<> sleepDist(1, 5);
        std::uniform_int_distribution<> forRepeatsDist(2, 5);
        
        std::vector<std::pair<std::string, int>> instructionWeights = {
            {"DECLARE", 15},   
            {"ADD", 20},       
            {"SUBTRACT", 15},    
            {"PRINT", 25},     
            {"SLEEP", 10},      
            {"FOR", 15}         
        };
        
        std::vector<std::string> weightedInstructions;
        for (const auto& pair : instructionWeights) {
            for (int i = 0; i < pair.second; ++i) {
                weightedInstructions.push_back(pair.first);
            }
        }
        std::uniform_int_distribution<> instDist(0, weightedInstructions.size() - 1);
        
        std::set<std::string> declaredVars;
        int currentNestingLevel = 0; 
        
        for (int i = 0; i < count; ++i) {
            std::string instructionType = weightedInstructions[instDist(gen)];
            std::string instruction;
            
            if (instructionType == "DECLARE") {
                std::string var = varNames[varDist(gen)];
                int value = valueDist(gen);
                instruction = "DECLARE(" + var + ", " + std::to_string(value) + ")";
                declaredVars.insert(var);
                
            } else if (instructionType == "ADD") {
                std::string var1 = varNames[varDist(gen)];
                std::string var2 = varNames[varDist(gen)];
                
                if (gen() % 2 == 0) {
                    int value = valueDist(gen);
                    instruction = "ADD(" + var1 + ", " + var2 + ", " + std::to_string(value) + ")";
                } else {
                    std::string var3 = varNames[varDist(gen)];
                    instruction = "ADD(" + var1 + ", " + var2 + ", " + var3 + ")";
                }
                declaredVars.insert(var1);
                
            } else if (instructionType == "SUBTRACT") {
                std::string var1 = varNames[varDist(gen)];
                std::string var2 = varNames[varDist(gen)];
                
                if (gen() % 2 == 0) {
                    int value = valueDist(gen);
                    instruction = "SUBTRACT(" + var1 + ", " + var2 + ", " + std::to_string(value) + ")";
                } else {
                    std::string var3 = varNames[varDist(gen)];
                    instruction = "SUBTRACT(" + var1 + ", " + var2 + ", " + var3 + ")";
                }
                declaredVars.insert(var1);
                
            } else if (instructionType == "PRINT") {
                if (!declaredVars.empty() && gen() % 3 == 0) {
                    auto it = declaredVars.begin();
                    std::advance(it, gen() % declaredVars.size());
                    std::string var = *it;
                    instruction = "PRINT(\"Hello world from " + name + "!\" + " + var + ")";
                } else {
                    instruction = "PRINT(\"Hello world from " + name + "!\")";
                }
                
            } else if (instructionType == "SLEEP") {
                int ticks = sleepDist(gen);
                instruction = "SLEEP(" + std::to_string(ticks) + ")";
                
            } else if (instructionType == "FOR") {
                if (currentNestingLevel < 3) {
                    currentNestingLevel++;
                    
                    int repeats = forRepeatsDist(gen);
                    int innerInstructions = std::min(2, count - i - 1);
                    
                    std::vector<std::string> forBody;
                    for (int j = 0; j < innerInstructions; ++j) {
                        std::string innerType = weightedInstructions[instDist(gen)];
                        
                        if (innerType == "FOR" && currentNestingLevel >= 3) {
                            innerType = "ADD"; 
                        }
                        
                        std::string innerInstruction;
                        if (innerType == "ADD") {
                            std::string var = "counter";
                            innerInstruction = "ADD(" + var + ", " + var + ", 1)";
                        } else if (innerType == "PRINT") {
                            innerInstruction = "PRINT(\"Hello world from " + name + "!\")";
                        } else if (innerType == "DECLARE") {
                            std::string var = varNames[varDist(gen)];
                            int value = valueDist(gen);
                            innerInstruction = "DECLARE(" + var + ", " + std::to_string(value) + ")";
                        } else if (innerType == "FOR" && currentNestingLevel < 3) {
                            innerInstruction = "FOR([ADD(counter, counter, 1)], 2)";
                            currentNestingLevel++; 
                        } else {
                            innerInstruction = "ADD(counter, counter, 1)";
                        }
                        forBody.push_back(innerInstruction);
                    }
                    
                    instruction = "FOR([";
                    for (size_t k = 0; k < forBody.size(); ++k) {
                        instruction += forBody[k];
                        if (k < forBody.size() - 1) instruction += ", ";
                    }
                    instruction += "], " + std::to_string(repeats) + ")";
                    
                    currentNestingLevel--;
                } else {
                    std::string var1 = varNames[varDist(gen)];
                    std::string var2 = varNames[varDist(gen)];
                    int value = valueDist(gen);
                    instruction = "ADD(" + var1 + ", " + var2 + ", " + std::to_string(value) + ")";
                    declaredVars.insert(var1);
                }
            }
            
            pendingInstructions.push(instruction);
        }
    }
    
    std::string executeNextInstruction() {
        if (pendingInstructions.empty()) {
            return "";
        }
        
        std::string instruction = pendingInstructions.front();
        pendingInstructions.pop();
        
        std::string timestamp = Utils::getCurrentTimestamp();
        std::string result = processInstruction(instruction);
        std::string logEntry = "(" + timestamp + ") Core:" + std::to_string(coreAssignment) + " " + instruction;
        
        if (!result.empty()) {
            logEntry += " -> " + result;
        }
        
        instructionHistory.push_back(logEntry);
        executedInstructions++;
        remainingTime--;
        
        if (responseTime == -1) {
            responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count() - arrivalTime;
        }
        
        return logEntry;
    }
    
private:
    std::map<std::string, uint16_t> variables;
    
    std::string processInstruction(const std::string& instruction) {
        if (instruction.find("DECLARE(") == 0) {
            return processDeclare(instruction);
        } else if (instruction.find("ADD(") == 0) {
            return processAdd(instruction);
        } else if (instruction.find("SUBTRACT(") == 0) {
            return processSubtract(instruction);
        } else if (instruction.find("PRINT(") == 0) {
            return processPrint(instruction);
        } else if (instruction.find("SLEEP(") == 0) {
            return processSleep(instruction);
        } else if (instruction.find("FOR(") == 0) {
            return processFor(instruction);
        }
        return "";
    }
    
    std::string processDeclare(const std::string& instruction) {
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string params = instruction.substr(start, end - start);
        
        size_t comma = params.find(',');
        if (comma != std::string::npos) {
            std::string varName = params.substr(0, comma);
            std::string valueStr = params.substr(comma + 1);
            
            varName.erase(std::remove_if(varName.begin(), varName.end(), ::isspace), varName.end());
            valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
            
            int value = std::stoi(valueStr);
            uint16_t clampedValue = static_cast<uint16_t>(std::max(0, std::min(value, static_cast<int>(UINT16_MAX))));
            variables[varName] = clampedValue;
            
            return "Declared " + varName + " = " + std::to_string(clampedValue);
        }
        return "";
    }
    
    std::string processAdd(const std::string& instruction) {
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string params = instruction.substr(start, end - start);
        
        std::vector<std::string> tokens;
        std::stringstream ss(params);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
            tokens.push_back(token);
        }
        
        if (tokens.size() == 3) {
            std::string var1 = tokens[0];
            uint16_t val2 = getVariableOrValue(tokens[1]);
            uint16_t val3 = getVariableOrValue(tokens[2]);
            
            uint32_t result32 = static_cast<uint32_t>(val2) + static_cast<uint32_t>(val3);
            uint16_t result = static_cast<uint16_t>(std::min(result32, static_cast<uint32_t>(UINT16_MAX)));
            
            variables[var1] = result;
            
            return var1 + " = " + std::to_string(val2) + " + " + std::to_string(val3) + " = " + std::to_string(result);
        }
        return "";
    }
    
    std::string processSubtract(const std::string& instruction) {
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string params = instruction.substr(start, end - start);
        
        std::vector<std::string> tokens;
        std::stringstream ss(params);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
            tokens.push_back(token);
        }
        
        if (tokens.size() == 3) {
            std::string var1 = tokens[0];
            uint16_t val2 = getVariableOrValue(tokens[1]);
            uint16_t val3 = getVariableOrValue(tokens[2]);
            
            uint16_t result = (val2 > val3) ? (val2 - val3) : 0;
            
            variables[var1] = result;
            
            return var1 + " = " + std::to_string(val2) + " - " + std::to_string(val3) + " = " + std::to_string(result);
        }
        return "";
    }
    
    std::string processPrint(const std::string& instruction) {
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string content = instruction.substr(start, end - start);
        
        if (content.find(" + ") != std::string::npos) {
            size_t plusPos = content.find(" + ");
            std::string msgPart = content.substr(0, plusPos);
            std::string varPart = content.substr(plusPos + 3);
            
            if (msgPart.front() == '"' && msgPart.back() == '"') {
                msgPart = msgPart.substr(1, msgPart.length() - 2);
            }
            
            uint16_t varValue = getVariableOrValue(varPart);
            return "OUTPUT: " + msgPart + std::to_string(varValue);
        } else {
            if (content.front() == '"' && content.back() == '"') {
                content = content.substr(1, content.length() - 2);
            }
            return "OUTPUT: " + content;
        }
    }
    
    std::string processSleep(const std::string& instruction) {
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string ticksStr = instruction.substr(start, end - start);
        
        int ticks = std::stoi(ticksStr);
        return "Sleeping for " + std::to_string(ticks) + " CPU ticks";
    }
    
    std::string processFor(const std::string& instruction) {
        size_t repeatStart = instruction.rfind(',') + 1;
        size_t repeatEnd = instruction.find(')', repeatStart);
        std::string repeatsStr = instruction.substr(repeatStart, repeatEnd - repeatStart);
        repeatsStr.erase(std::remove_if(repeatsStr.begin(), repeatsStr.end(), ::isspace), repeatsStr.end());
        
        int repeats = std::stoi(repeatsStr);
        return "Executing FOR loop " + std::to_string(repeats) + " times";
    }
    
    uint16_t getVariableOrValue(const std::string& token) {
        if (std::isdigit(token[0])) {
            int value = std::stoi(token);
            return static_cast<uint16_t>(std::max(0, std::min(value, static_cast<int>(UINT16_MAX))));
        } else {
            if (variables.find(token) == variables.end()) {
                variables[token] = 0;
            }
            return variables[token];
        }
    }
    
public:
    
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
    
    ~Process() {
        variables.clear();
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

class Scheduler;

class Scheduler {
private:
    std::unique_ptr<SystemConfig> config;
    
    std::vector<std::shared_ptr<Process>> allProcesses;
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> runningProcesses; 
    std::vector<std::shared_ptr<Process>> terminatedProcesses;
    
    std::vector<std::thread> coreWorkers;
    std::atomic<bool> isRunning{false};
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> dummyProcessGenerationEnabled{false};
    mutable std::mutex processMutex;
    std::condition_variable processCV;
    
    std::atomic<int> processCounter{1};
    std::chrono::high_resolution_clock::time_point systemStartTime;
    
    std::vector<int> coreQuantumCounters;
    
    void coreWorkerThread(int coreId) {
        while (!shouldStop.load()) {
            std::shared_ptr<Process> currentProcess = nullptr;
            
            {
                std::unique_lock<std::mutex> lock(processMutex);
                
                currentProcess = runningProcesses[coreId];
                
                if (!currentProcess && !readyQueue.empty()) {
                    currentProcess = readyQueue.front();
                    readyQueue.pop();
                    
                    runningProcesses[coreId] = currentProcess;
                    currentProcess->state = ProcessState::RUNNING;
                    currentProcess->coreAssignment = coreId;
                }
            }
            
            if (currentProcess) {
                int instructionsPerChunk = 1; 
                int effectiveDelay = config->delayPerExec; 
                
                if (config->delayPerExec <= 5) {
                    instructionsPerChunk = 8; 
                }
                
                int instructionsExecuted = 0;
                while (instructionsExecuted < instructionsPerChunk && !currentProcess->isComplete()) {
                    std::string logEntry = currentProcess->executeNextInstruction();
                    instructionsExecuted++;
                    
                    if (currentProcess->isComplete()) {
                        break;
                    }
                }
                
                if (currentProcess->isComplete()) {
                    handleProcessCompletion(currentProcess);
                    
                    std::lock_guard<std::mutex> lock(processMutex);
                    runningProcesses[coreId] = nullptr;
                }
                else if (config->scheduler == "rr") {
                    coreQuantumCounters[coreId] += instructionsExecuted;
                    
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
                
                if (effectiveDelay > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(effectiveDelay));
                } else {
                    std::this_thread::yield();
                }
            } else {
                std::unique_lock<std::mutex> lock(processMutex);
                processCV.wait_for(lock, std::chrono::milliseconds(50));
            }
        }
    }
    
    void processCreatorThread() {
        while (!shouldStop.load()) {
            if (dummyProcessGenerationEnabled.load()) {
                int activeCores = 0;
                int queueSize = 0;
                
                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    for (const auto& process : runningProcesses) {
                        if (process != nullptr) activeCores++;
                    }
                    queueSize = readyQueue.size();
                }
                
                int processesToCreate = 1; 
                
                if (config->delayPerExec <= 5) {
                    int totalWorkload = activeCores + queueSize;
                    int desiredWorkload = config->numCpu + 5; 
                    
                    if (totalWorkload < desiredWorkload) {
                        processesToCreate = desiredWorkload - totalWorkload;
                    }
                    
                    if (config->delayPerExec == 0 && totalWorkload < config->numCpu * 2) {
                        processesToCreate = std::max(processesToCreate, 2);
                    }
                }
                
                for (int i = 0; i < processesToCreate; ++i) {
                    createProcess();
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(config->batchProcessFreq));
        }
    }
    
    void testModeProcessCreator() {
        while (!shouldStop.load()) {
            int activeCores = 0;
            int queueSize = 0;
            
            {
                std::lock_guard<std::mutex> lock(processMutex);
                for (const auto& process : runningProcesses) {
                    if (process != nullptr) activeCores++;
                }
                queueSize = readyQueue.size();
            }
            
            int totalWorkload = activeCores + queueSize;
            int desiredWorkload = config->numCpu * 2;
            
            int processesToCreate = std::max(1, desiredWorkload - totalWorkload);
            
            for (int i = 0; i < processesToCreate; ++i) {
                createProcess();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    
    void handleProcessCompletion(std::shared_ptr<Process> process) {
        process->state = ProcessState::TERMINATED;
        process->updateMetrics();
        process->coreAssignment = -1;
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            terminatedProcesses.push_back(process);
        }
    }
    
public:
    Scheduler(std::unique_ptr<SystemConfig> cfg) 
        : config(std::move(cfg)) {
        
        runningProcesses.resize(config->numCpu, nullptr);
        coreQuantumCounters.resize(config->numCpu, 0);
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
        
        coreWorkers.clear();
        for (int i = 0; i < config->numCpu; ++i) {
            coreWorkers.emplace_back(&Scheduler::coreWorkerThread, this, i);
        }
        
        coreWorkers.emplace_back(&Scheduler::processCreatorThread, this);
        
        std::cout << "Scheduler started with " << config->numCpu << " CPU cores." << std::endl;
        return true;
    }
    
    bool startTestMode() {
        if (isRunning.load()) {
            std::cout << "Scheduler is already running." << std::endl;
            return false;
        }
        
        shouldStop.store(false);
        isRunning.store(true);
        
        coreWorkers.clear();
        for (int i = 0; i < config->numCpu; ++i) {
            coreWorkers.emplace_back(&Scheduler::coreWorkerThread, this, i);
        }
        
        coreWorkers.emplace_back(&Scheduler::testModeProcessCreator, this);
        
        std::cout << "Scheduler test mode started with " << config->numCpu << " CPU cores." << std::endl;
        return true;
    }
    
    void stop() {
        if (!isRunning.load()) {
            return;
        }
        
        shouldStop.store(true);
        isRunning.store(false);
        
        processCV.notify_all();
        
        for (auto& worker : coreWorkers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        coreWorkers.clear();
        std::cout << "Scheduler stopped successfully." << std::endl;
    }
    
    bool createProcess(const std::string& name = "") {
        ensureSchedulerStarted();
        
        std::string processName = name;
        if (processName.empty()) {
            processName = "process" + std::to_string(processCounter.fetch_add(1));
        }
        
        auto process = std::make_shared<Process>(processName);
        
        int baseInstructionCount = Utils::generateRandomInt(
            config->minInstructions, config->maxInstructions);
        
        int instructionCount = baseInstructionCount;
        
        process->generateInstructions(instructionCount);
        
        process->state = ProcessState::READY;
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            allProcesses.push_back(process);
            readyQueue.push(process);
        }
        
        processCV.notify_one();
        
        return true;
    }
    
    void displaySystemStatus() const {
        std::lock_guard<std::mutex> lock(processMutex);
        
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
        
        bool hasRunningProcesses = false;
        for (size_t i = 0; i < runningProcesses.size(); ++i) {
            if (runningProcesses[i]) {
                auto& p = runningProcesses[i];
                std::cout << std::left << std::setw(12) << p->name << "  ";
                std::cout << "(Started: " << p->creationTimestamp << ")  ";
                std::cout << "Core: " << i << "  ";
                std::cout << p->executedInstructions << " / " << p->totalInstructions << std::endl;
                hasRunningProcesses = true;
            }
        }
        
        if (!hasRunningProcesses) {
            std::cout << "No processes currently running." << std::endl;
        }
        
        std::cout << std::endl << "Finished processes:" << std::endl;
        if (terminatedProcesses.empty()) {
            std::cout << "No processes have finished yet." << std::endl;
        } else {
            for (const auto& p : terminatedProcesses) {
                std::cout << std::left << std::setw(12) << p->name << "  ";
                std::cout << "(" << p->completionTimestamp << ")  ";
                std::cout << "Finished  ";
                std::cout << p->executedInstructions << " / " << p->totalInstructions << std::endl;
            }
        }
        std::cout << "---------------------------------------------" << std::endl;
    }
    
    void generateReport(const std::string& filename) const {
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
        
        file << "CSOPESY OS Emulator Report" << std::endl;
        file << "Generated: " << Utils::getCurrentTimestamp() << std::endl;
        file << std::endl;
        
        int busyCores = 0;
        for (const auto& process : runningProcesses) {
            if (process != nullptr) busyCores++;
        }
        
        int totalCores = config->numCpu;
        int coresAvailable = totalCores - busyCores;
        double cpuUtilization = (static_cast<double>(busyCores) / totalCores) * 100.0;
        
        file << "---------------------------------------------" << std::endl;
        file << "CPU Status:" << std::endl;
        file << "Total Cores      : " << totalCores << std::endl;
        file << "Cores Used       : " << busyCores << std::endl;
        file << "Cores Available  : " << coresAvailable << std::endl;
        file << "CPU Utilization  : " << static_cast<int>(cpuUtilization) << "%" << std::endl;
        
        file << "\n---------------------------------------------" << std::endl;
        file << "Running processes:" << std::endl;
        bool hasRunningProcesses = false;
        for (size_t i = 0; i < runningProcesses.size(); ++i) {
            if (runningProcesses[i]) {
                auto& p = runningProcesses[i];
                file << std::left << std::setw(12) << p->name << "  ";
                file << "(Started: " << p->creationTimestamp << ")  ";
                file << "Core: " << i << "  ";
                file << p->executedInstructions << " / " << p->totalInstructions << std::endl;
                hasRunningProcesses = true;
            }
        }
        
        if (!hasRunningProcesses) {
            file << "No processes currently running." << std::endl;
        }
        
        file << "\nFinished processes:" << std::endl;
        if (terminatedProcesses.empty()) {
            file << "No processes have finished yet." << std::endl;
        } else {
            for (const auto& p : terminatedProcesses) {
                file << std::left << std::setw(12) << p->name << "  ";
                file << "(" << p->completionTimestamp << ")  ";
                file << "Finished  ";
                file << p->executedInstructions << " / " << p->totalInstructions << std::endl;
            }
        }
        file << "---------------------------------------------" << std::endl;
        
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
    
    void ensureSchedulerStarted() {
        if (!isRunning.load()) {
            start();
        }
    }
    
    void enableDummyProcessGeneration() {
        dummyProcessGenerationEnabled.store(true);
    }
    
    void disableDummyProcessGeneration() {
        dummyProcessGenerationEnabled.store(false);
        std::cout << "Dummy process generation disabled." << std::endl;
    }
    
    bool isDummyGenerationEnabled() const { return dummyProcessGenerationEnabled.load(); }
};

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
            
            std::string filename = "csopesy-log.txt";
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
                << "|  scheduler-test   - Start scheduler in test mode for performance testing.      |\n"
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
    
    void run() {
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
                std::cout << "Goodbye!" << std::endl;
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
    
    void displayHeader() {
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
};

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
