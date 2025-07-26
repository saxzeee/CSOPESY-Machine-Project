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
    size_t maxOverallMem = 1073741824; 
    size_t memPerFrame = 4096; 
    size_t minMemPerProc = 65536; 
    size_t maxMemPerProc = 1048576; 
    
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
                else if (key == "max-overall-mem") maxOverallMem = std::stoull(value);
                else if (key == "mem-per-frame") memPerFrame = std::stoull(value);
                else if (key == "min-mem-per-proc") minMemPerProc = std::stoull(value);
                else if (key == "max-mem-per-proc") maxMemPerProc = std::stoull(value);
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
        std::cout << "Memory per Frame      : " << memPerFrame << std::endl;
        std::cout << "Min Memory per Process: " << minMemPerProc << std::endl;
        std::cout << "Max Memory per Process: " << maxMemPerProc << std::endl;
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
    
    bool isPowerOfTwo(size_t n) {
        return n > 0 && (n & (n - 1)) == 0;
    }
    
    bool isValidMemorySize(size_t memorySize) {
        return memorySize >= 64 && memorySize <= 65536 && isPowerOfTwo(memorySize);
    }
    
    std::string toHex(uint32_t value) {
        std::stringstream ss;
        ss << std::hex << std::uppercase << value;
        return ss.str();
    }
}

class MemoryManager {
private:
    size_t totalMemory;
    size_t memPerFrame;
    size_t totalFrames;
    std::vector<bool> frameAllocation;
    std::map<std::string, std::vector<int>> processFrames;
    std::map<std::string, std::set<int>> processVirtualPages;
    std::map<std::string, std::map<int, int>> virtualToPhysicalMap;
    std::queue<int> freeFrames;
    std::queue<int> frameQueue;
    std::string backingStoreFile;
    mutable std::mutex memoryMutex;
    
    std::atomic<long long> totalCpuTicks{0};
    std::atomic<long long> idleCpuTicks{0};
    std::atomic<long long> activeCpuTicks{0};
    std::atomic<long long> pageInCount{0};
    std::atomic<long long> pageOutCount{0};
    std::atomic<long long> pageFaultCount{0};
    
public:
    MemoryManager(size_t totalMem, size_t frameSize) 
        : totalMemory(totalMem), memPerFrame(frameSize), backingStoreFile("csopesy-backing-store.txt") {
        totalFrames = totalMem / frameSize;
        frameAllocation.resize(totalFrames, false);
        
        for (size_t i = 0; i < totalFrames; ++i) {
            freeFrames.push(static_cast<int>(i));
        }
        
        std::ofstream backingStore(backingStoreFile, std::ios::trunc);
        if (backingStore.is_open()) {
            backingStore << "CSOPESY Backing Store - Memory Pages\n";
            backingStore << "====================================\n\n";
            backingStore.close();
        }
    }
    
    bool allocateMemory(const std::string& processId, int pagesNeeded) {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        for (int i = 0; i < pagesNeeded; ++i) {
            processVirtualPages[processId].insert(i);
        }
        
        return true;
    }
    
    bool accessPage(const std::string& processId, int virtualPage) {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        if (processVirtualPages[processId].find(virtualPage) == processVirtualPages[processId].end()) {
            return false;
        }
        
        if (virtualToPhysicalMap[processId].find(virtualPage) != virtualToPhysicalMap[processId].end()) {
            return true;
        }
        
        pageFaultCount.fetch_add(1);
        
        int physicalFrame;
        if (!freeFrames.empty()) {
            physicalFrame = freeFrames.front();
            freeFrames.pop();
        } else {
            physicalFrame = evictPage();
        }
        
        loadPageFromBackingStore(processId, virtualPage, physicalFrame);
        virtualToPhysicalMap[processId][virtualPage] = physicalFrame;
        frameAllocation[physicalFrame] = true;
        processFrames[processId].push_back(physicalFrame);
        frameQueue.push(physicalFrame);
        pageInCount.fetch_add(1);
        
        return true;
    }
    
    int evictPage() {
        if (frameQueue.empty()) {
            return 0;
        }
        
        int frameToEvict = frameQueue.front();
        frameQueue.pop();
        
        for (auto& processEntry : virtualToPhysicalMap) {
            for (auto& pageEntry : processEntry.second) {
                if (pageEntry.second == frameToEvict) {
                    savePageToBackingStore(processEntry.first, pageEntry.first, frameToEvict);
                    processEntry.second.erase(pageEntry.first);
                    
                    auto& frames = processFrames[processEntry.first];
                    frames.erase(std::remove(frames.begin(), frames.end(), frameToEvict), frames.end());
                    pageOutCount.fetch_add(1);
                    
                    return frameToEvict;
                }
            }
        }
        
        return frameToEvict;
    }
    
    void loadPageFromBackingStore(const std::string& processId, int virtualPage, int physicalFrame) {
        std::ifstream backingStore(backingStoreFile);
        std::string line;
        bool found = false;
        
        std::string searchPattern = "Process: " + processId + " Page: " + std::to_string(virtualPage);
        
        while (std::getline(backingStore, line)) {
            if (line.find(searchPattern) != std::string::npos) {
                found = true;
                break;
            }
        }
        backingStore.close();
        
        if (!found) {
            std::ofstream backingStoreOut(backingStoreFile, std::ios::app);
            if (backingStoreOut.is_open()) {
                backingStoreOut << "Process: " << processId << " Page: " << virtualPage 
                              << " -> Frame: " << physicalFrame << " (Initial Load)\n";
                backingStoreOut.close();
            }
        }
    }
    
    void savePageToBackingStore(const std::string& processId, int virtualPage, int physicalFrame) {
        std::ofstream backingStore(backingStoreFile, std::ios::app);
        if (backingStore.is_open()) {
            backingStore << "Process: " << processId << " Page: " << virtualPage 
                        << " -> Evicted from Frame: " << physicalFrame << " at " 
                        << std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count() << "ms\n";
            backingStore.close();
        }
    }
    
    void deallocateMemory(const std::string& processId) {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        if (processFrames.find(processId) != processFrames.end()) {
            for (int frame : processFrames[processId]) {
                frameAllocation[frame] = false;
                freeFrames.push(frame);
            }
            processFrames.erase(processId);
        }
        
        if (virtualToPhysicalMap.find(processId) != virtualToPhysicalMap.end()) {
            int pagesFreed = static_cast<int>(virtualToPhysicalMap[processId].size());
            virtualToPhysicalMap.erase(processId);
            pageOutCount.fetch_add(pagesFreed);
        }
        
        processVirtualPages.erase(processId);
        
        std::ofstream backingStore(backingStoreFile, std::ios::app);
        if (backingStore.is_open()) {
            backingStore << "Process: " << processId << " - All pages deallocated\n";
            backingStore.close();
        }
    }
    
    size_t getUsedMemory() const {
        std::lock_guard<std::mutex> lock(memoryMutex);
        size_t usedFrames = 0;
        for (bool allocated : frameAllocation) {
            if (allocated) usedFrames++;
        }
        return usedFrames * memPerFrame;
    }
    
    size_t getFreeMemory() const {
        return totalMemory - getUsedMemory();
    }
    
    size_t getTotalMemory() const { return totalMemory; }
    size_t getTotalFrames() const { return totalFrames; }
    size_t getUsedFrames() const {
        std::lock_guard<std::mutex> lock(memoryMutex);
        size_t usedFrames = 0;
        for (bool allocated : frameAllocation) {
            if (allocated) usedFrames++;
        }
        return usedFrames;
    }
    
    void incrementCpuTicks(bool wasIdle) {
        totalCpuTicks.fetch_add(1);
        if (wasIdle) {
            idleCpuTicks.fetch_add(1);
        } else {
            activeCpuTicks.fetch_add(1);
        }
    }
    
    long long getTotalCpuTicks() const { return totalCpuTicks.load(); }
    long long getIdleCpuTicks() const { return idleCpuTicks.load(); }
    long long getActiveCpuTicks() const { return activeCpuTicks.load(); }
    long long getPageInCount() const { return pageInCount.load(); }
    long long getPageOutCount() const { return pageOutCount.load(); }
    long long getPageFaultCount() const { return pageFaultCount.load(); }
    size_t getFrameSize() const { return memPerFrame; }
    
    std::map<std::string, std::vector<int>> getProcessFrames() const {
        std::lock_guard<std::mutex> lock(memoryMutex);
        return processFrames;
    }
    
    std::map<std::string, std::set<int>> getProcessVirtualPages() const {
        std::lock_guard<std::mutex> lock(memoryMutex);
        return processVirtualPages;
    }
    
    bool ensurePageResident(const std::string& processId, uint32_t virtualAddress) {
        std::lock_guard<std::mutex> lock(memoryMutex);
        
        int virtualPage = virtualAddress / memPerFrame;
        
        if (virtualToPhysicalMap[processId].find(virtualPage) != virtualToPhysicalMap[processId].end()) {
            return true;
        }
        
        pageFaultCount.fetch_add(1);
        
        int physicalFrame;
        if (!freeFrames.empty()) {
            physicalFrame = freeFrames.front();
            freeFrames.pop();
        } else {
            physicalFrame = evictPage();
        }
        
        loadPageFromBackingStore(processId, virtualPage, physicalFrame);
        virtualToPhysicalMap[processId][virtualPage] = physicalFrame;
        frameAllocation[physicalFrame] = true;
        processFrames[processId].push_back(physicalFrame);
        frameQueue.push(physicalFrame);
        pageInCount.fetch_add(1);
        
        return true;
    }
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
    int sleepRemaining = 0;
    size_t memoryRequired = 0;  
    int pagesRequired = 0;
    MemoryManager* memoryManager = nullptr;
    
    std::map<uint32_t, uint16_t> processMemory;
    static const size_t SYMBOL_TABLE_SIZE = 64;
    static const size_t MAX_VARIABLES = 32;      
    
    bool memoryAccessViolation = false;
    std::string violationTimestamp;
    std::string violationAddress;      
    
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
            {"DECLARE", 12},   
            {"ADD", 15},       
            {"SUBTRACT", 12},    
            {"PRINT", 20},     
            {"SLEEP", 8},      
            {"FOR", 12},
            {"READ", 10},
            {"WRITE", 11}         
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
                
            } else if (instructionType == "READ") {
                if (!declaredVars.empty() || gen() % 3 == 0) {
                    std::string var = varNames[varDist(gen)];
                    uint32_t address = 0x1000 + (gen() % 4096) * 2;
                    instruction = "READ(" + var + ", 0x" + Utils::toHex(address) + ")";
                    declaredVars.insert(var);
                } else {
                    instruction = "DECLARE(x, 0)";
                    declaredVars.insert("x");
                }
                
            } else if (instructionType == "WRITE") {
                uint32_t address = 0x1000 + (gen() % 4096) * 2;
                uint16_t value = valueDist(gen);
                instruction = "WRITE(0x" + Utils::toHex(address) + ", " + std::to_string(value) + ")";
            }
            
            pendingInstructions.push(instruction);
        }
    }
    
    void setCustomInstructions(const std::vector<std::string>& customInstructions) {
        totalInstructions = customInstructions.size();
        remainingTime = customInstructions.size();
        burstTime = customInstructions.size();
        
        while (!pendingInstructions.empty()) {
            pendingInstructions.pop();
        }
        
        for (const auto& instruction : customInstructions) {
            pendingInstructions.push(instruction);
        }
    }
    
    std::string executeNextInstruction() {
        if (pendingInstructions.empty()) {
            return "";
        }
        
        if (memoryManager && pagesRequired > 0) {
            int pageToAccess = Utils::generateRandomInt(0, pagesRequired - 1);
            memoryManager->accessPage(pid, pageToAccess);
        }
        
        std::string instruction = pendingInstructions.front();
        
        std::string timestamp = Utils::getCurrentTimestamp();
        std::string result = processInstruction(instruction);
        
        if (result.find("Page fault occurred") != std::string::npos) {
            return result;
        }
        
        pendingInstructions.pop();
        
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
        } else if (instruction.find("READ(") == 0) {
            return processRead(instruction);
        } else if (instruction.find("WRITE(") == 0) {
            return processWrite(instruction);
        }
        return "";
    }
    
    std::string processDeclare(const std::string& instruction) {
        if (variables.size() >= MAX_VARIABLES) {
            return "Variable limit reached (32 max). Declaration ignored.";
        }
        
        if (!ensureSymbolTableResident()) {
            return "Page fault occurred while accessing symbol table. Retrying...";
        }
        
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
        if (!ensureSymbolTableResident()) {
            return "Page fault occurred while accessing symbol table. Retrying...";
        }
        
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
        if (!ensureSymbolTableResident()) {
            return "Page fault occurred while accessing symbol table. Retrying...";
        }
        
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
        if (!ensureSymbolTableResident()) {
            return "Page fault occurred while accessing symbol table. Retrying...";
        }
        
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
        sleepRemaining = ticks;
        state = ProcessState::WAITING;
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
    
    bool isValidMemoryAddress(uint32_t address) {
        return address >= 0x1000 && address < (0x1000 + memoryRequired);
    }
    
    void recordMemoryAccessViolation(const std::string& address) {
        memoryAccessViolation = true;
        violationAddress = address;
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        violationTimestamp = oss.str();
        
        state = ProcessState::TERMINATED;
    }
    
    bool ensureSymbolTableResident() {
        if (!memoryManager) return true;
        
        uint32_t symbolTableAddress = 0x1000;
        return memoryManager->ensurePageResident(pid, symbolTableAddress);
    }
    
    bool ensureAddressResident(uint32_t address) {
        if (!memoryManager) return true;
        
        return memoryManager->ensurePageResident(pid, address);
    }
    
    std::string processRead(const std::string& instruction) {
        if (variables.size() >= MAX_VARIABLES) {
            return "Variable limit reached (32 max). READ ignored.";
        }
        
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string params = instruction.substr(start, end - start);
        
        size_t comma = params.find(',');
        if (comma != std::string::npos) {
            std::string varName = params.substr(0, comma);
            std::string addressStr = params.substr(comma + 1);
            
            varName.erase(std::remove_if(varName.begin(), varName.end(), ::isspace), varName.end());
            addressStr.erase(std::remove_if(addressStr.begin(), addressStr.end(), ::isspace), addressStr.end());
            
            uint32_t address;
            try {
                address = std::stoul(addressStr, nullptr, 16);
            } catch (const std::exception& e) {
                recordMemoryAccessViolation(addressStr);
                return "ACCESS VIOLATION: Invalid address format. Process terminated.";
            }
            
            if (!isValidMemoryAddress(address)) {
                recordMemoryAccessViolation(addressStr);
                return "ACCESS VIOLATION: Address " + addressStr + " outside process memory space. Process terminated.";
            }
            
            if (!ensureAddressResident(address)) {
                return "Page fault occurred while accessing memory. Retrying...";
            }
            
            uint16_t value = 0;
            auto memIt = processMemory.find(address);
            if (memIt != processMemory.end()) {
                value = memIt->second;
            }
            
            if (!ensureSymbolTableResident()) {
                return "Page fault occurred while accessing symbol table. Retrying...";
            }
            
            variables[varName] = value;
            return "Read " + varName + " = " + std::to_string(value) + " from " + addressStr;
        }
        return "";
    }
    
    std::string processWrite(const std::string& instruction) {
        size_t start = instruction.find('(') + 1;
        size_t end = instruction.find(')', start);
        std::string params = instruction.substr(start, end - start);
        
        size_t comma = params.find(',');
        if (comma != std::string::npos) {
            std::string addressStr = params.substr(0, comma);
            std::string valueStr = params.substr(comma + 1);
            
            addressStr.erase(std::remove_if(addressStr.begin(), addressStr.end(), ::isspace), addressStr.end());
            valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
            
            uint32_t address;
            try {
                address = std::stoul(addressStr, nullptr, 16);
            } catch (const std::exception& e) {
                recordMemoryAccessViolation(addressStr);
                return "ACCESS VIOLATION: Invalid address format. Process terminated.";
            }
            
            if (!isValidMemoryAddress(address)) {
                recordMemoryAccessViolation(addressStr);
                return "ACCESS VIOLATION: Address " + addressStr + " outside process memory space. Process terminated.";
            }
            
            if (!ensureAddressResident(address)) {
                return "Page fault occurred while accessing memory. Retrying...";
            }
            
            uint16_t value;
            try {
                int val = std::stoi(valueStr);
                value = static_cast<uint16_t>(std::max(0, std::min(val, static_cast<int>(UINT16_MAX))));
            } catch (const std::exception& e) {
                return "Invalid value format for WRITE instruction.";
            }
            
            processMemory[address] = value;
            return "Wrote " + std::to_string(value) + " to " + addressStr;
        }
        return "";
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

class Process;
class Scheduler;

class Scheduler {
private:
    std::unique_ptr<SystemConfig> config;
    std::unique_ptr<MemoryManager> memoryManager;
    
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
                memoryManager->incrementCpuTicks(false); 
                
                if (currentProcess->sleepRemaining > 0) {
                    currentProcess->sleepRemaining--;
                    
                    if (currentProcess->sleepRemaining == 0) {
                        currentProcess->state = ProcessState::READY;
                        
                        std::lock_guard<std::mutex> lock(processMutex);
                        currentProcess->coreAssignment = -1;
                        readyQueue.push(currentProcess);
                        runningProcesses[coreId] = nullptr;
                        processCV.notify_one();
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(config->delayPerExec));
                    }
                    continue;
                }
                
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
                    
                    if (currentProcess->state == ProcessState::WAITING && currentProcess->sleepRemaining > 0) {
                        break;
                    }
                }
                
                if (currentProcess->isComplete()) {
                    handleProcessCompletion(currentProcess);
                    
                    std::lock_guard<std::mutex> lock(processMutex);
                    runningProcesses[coreId] = nullptr;
                }
                else if (currentProcess->state == ProcessState::WAITING && currentProcess->sleepRemaining > 0) {
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
                memoryManager->incrementCpuTicks(true); 
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
        
        memoryManager->deallocateMemory(process->pid);
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            terminatedProcesses.push_back(process);
        }
    }
    
public:
    Scheduler(std::unique_ptr<SystemConfig> cfg) 
        : config(std::move(cfg)) {
        
        memoryManager = std::make_unique<MemoryManager>(
            config->maxOverallMem, config->memPerFrame);
        
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
        
        size_t memoryRequired = Utils::generateRandomInt(
            static_cast<int>(config->minMemPerProc), 
            static_cast<int>(config->maxMemPerProc));
        
        int pagesRequired = static_cast<int>((memoryRequired + config->memPerFrame - 1) / config->memPerFrame);
        
        process->memoryRequired = memoryRequired;
        process->pagesRequired = pagesRequired;
        process->memoryManager = memoryManager.get();
        
        if (!memoryManager->allocateMemory(process->pid, pagesRequired)) {
            std::cout << "Warning: Cannot allocate virtual memory for process " << process->name << std::endl;
            process->memoryRequired = 0;
            process->pagesRequired = 0;
        }
        
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
    
    bool createProcess(const std::string& name, size_t customMemorySize) {
        ensureSchedulerStarted();
        
        if (!Utils::isValidMemorySize(customMemorySize)) {
            std::cout << "Invalid memory allocation. Memory size must be between 64 and 65536 bytes and a power of 2." << std::endl;
            return false;
        }
        
        std::string processName = name;
        if (processName.empty()) {
            processName = "process" + std::to_string(processCounter.fetch_add(1));
        }
        
        auto process = std::make_shared<Process>(processName);
        
        int baseInstructionCount = Utils::generateRandomInt(
            config->minInstructions, config->maxInstructions);
        
        int instructionCount = baseInstructionCount;
        
        size_t memoryRequired = customMemorySize;
        int pagesRequired = static_cast<int>((memoryRequired + config->memPerFrame - 1) / config->memPerFrame);
        
        process->memoryRequired = memoryRequired;
        process->pagesRequired = pagesRequired;
        process->memoryManager = memoryManager.get();
        
        if (!memoryManager->allocateMemory(process->pid, pagesRequired)) {
            std::cout << "Warning: Cannot allocate virtual memory for process " << process->name << std::endl;
            process->memoryRequired = 0;
            process->pagesRequired = 0;
        }
        
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
    
    std::vector<std::string> parseInstructions(const std::string& instructionString) {
        std::vector<std::string> instructions;
        std::string current = "";
        bool inQuotes = false;
        
        for (size_t i = 0; i < instructionString.length(); ++i) {
            char c = instructionString[i];
            
            if (c == '"') {
                inQuotes = !inQuotes;
                current += c;
            } else if (c == ';' && !inQuotes) {
                if (!current.empty()) {
                    std::string trimmed = current;
                    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
                    if (!trimmed.empty()) {
                        instructions.push_back(trimmed);
                    }
                    current = "";
                }
            } else {
                current += c;
            }
        }
        
        if (!current.empty()) {
            std::string trimmed = current;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
            if (!trimmed.empty()) {
                instructions.push_back(trimmed);
            }
        }
        
        return instructions;
    }
    
    bool createProcess(const std::string& name, size_t customMemorySize, const std::string& customInstructions) {
        ensureSchedulerStarted();
        
        if (!Utils::isValidMemorySize(customMemorySize)) {
            std::cout << "Invalid memory allocation. Memory size must be between 64 and 65536 bytes and a power of 2." << std::endl;
            return false;
        }
        
        std::vector<std::string> instructionList = parseInstructions(customInstructions);
        if (instructionList.size() < 1 || instructionList.size() > 50) {
            std::cout << "Invalid command. Instructions must be between 1 and 50." << std::endl;
            return false;
        }
        
        std::string processName = name;
        if (processName.empty()) {
            processName = "process" + std::to_string(processCounter.fetch_add(1));
        }
        
        auto process = std::make_shared<Process>(processName);
        
        size_t memoryRequired = customMemorySize;
        int pagesRequired = static_cast<int>((memoryRequired + config->memPerFrame - 1) / config->memPerFrame);
        
        process->memoryRequired = memoryRequired;
        process->pagesRequired = pagesRequired;
        process->memoryManager = memoryManager.get();
        
        if (!memoryManager->allocateMemory(process->pid, pagesRequired)) {
            std::cout << "Warning: Cannot allocate virtual memory for process " << process->name << std::endl;
            process->memoryRequired = 0;
            process->pagesRequired = 0;
        }
        
        process->setCustomInstructions(instructionList);
        
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
                std::cout << p->executedInstructions << " / " << p->totalInstructions << "  ";
                std::cout << "Memory: " << p->memoryRequired << " bytes (" << p->pagesRequired << " pages)" << std::endl;
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
                std::cout << p->executedInstructions << " / " << p->totalInstructions << "  ";
                std::cout << "Memory: " << p->memoryRequired << " bytes (" << p->pagesRequired << " pages)" << std::endl;
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
                file << p->executedInstructions << " / " << p->totalInstructions << "  ";
                file << "Memory: " << p->memoryRequired << " bytes (" << p->pagesRequired << " pages)" << std::endl;
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
                file << p->executedInstructions << " / " << p->totalInstructions << "  ";
                file << "Memory: " << p->memoryRequired << " bytes (" << p->pagesRequired << " pages)" << std::endl;
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
    
    MemoryManager* getMemoryManager() const { return memoryManager.get(); }
    
    std::vector<std::shared_ptr<Process>> getAllProcesses() const {
        std::lock_guard<std::mutex> lock(processMutex);
        return allProcesses;
    }
    
    std::vector<std::shared_ptr<Process>> getRunningProcesses() const {
        std::lock_guard<std::mutex> lock(processMutex);
        std::vector<std::shared_ptr<Process>> running;
        for (const auto& process : runningProcesses) {
            if (process != nullptr) {
                running.push_back(process);
            }
        }
        return running;
    }
    
    std::vector<std::shared_ptr<Process>> getTerminatedProcesses() const {
        std::lock_guard<std::mutex> lock(processMutex);
        return terminatedProcesses;
    }
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
    
    void displayProcessSmi() {
        auto memManager = scheduler->getMemoryManager();
        auto runningProcesses = scheduler->getRunningProcesses();
        auto allProcesses = scheduler->getAllProcesses();
        
        std::cout << "\n";
        std::cout << "+---------------------------------------------------------------------------------+\n";
        std::cout << "| PROCESS-SMI 1.0.0 Driver Version: 01.00.00       CSOPESY Version: 1.0.0      |\n";
        std::cout << "+---------------------------------------------------------------------------------+\n";
        
        int busyCores = static_cast<int>(runningProcesses.size());
        
        size_t totalMemory = memManager->getTotalMemory();
        size_t usedMemory = memManager->getUsedMemory();
        size_t freeMemory = memManager->getFreeMemory();
        double memoryUtilization = (static_cast<double>(usedMemory) / totalMemory) * 100.0;
        
        std::cout << "| Memory-Usage: " << usedMemory << "/" << totalMemory;
        std::cout << " (" << std::setw(3) << static_cast<int>(memoryUtilization) << "%)";
        std::cout << std::setw(40) << "|\n";
        
        std::cout << "+---------------------------------------------------------------------------------+\n";
        std::cout << "\nRunning processes and memory usage:\n";
        std::cout << "+---------------------------------------------------------------------------------+\n";
        
        if (runningProcesses.empty()) {
            std::cout << "| No processes currently running.                                                |\n";
        } else {
            for (const auto& process : runningProcesses) {
                std::cout << "| " << std::left << std::setw(12) << process->name;
                std::cout << " " << std::setw(8) << process->pid;
                std::cout << " " << std::setw(10) << (process->memoryRequired / 1024) << "KB";
                std::cout << " " << std::setw(8) << process->pagesRequired << "pages";
                std::cout << " Core:" << process->coreAssignment;
                std::cout << std::setw(25) << "|\n";
            }
        }
        
        std::cout << "+---------------------------------------------------------------------------------+\n";
        std::cout << "\n";
    }
    
    void displayVmstat() {
        auto memManager = scheduler->getMemoryManager();
        auto runningProcesses = scheduler->getRunningProcesses();
        auto allProcesses = scheduler->getAllProcesses();
        
        size_t totalMemoryKB = memManager->getTotalMemory() / 1024;
        size_t usedMemoryKB = memManager->getUsedMemory() / 1024;
        size_t freeMemoryKB = memManager->getFreeMemory() / 1024;
        
        size_t totalFrames = memManager->getTotalFrames();
        size_t usedFrames = memManager->getUsedFrames();
        size_t freeFrames = totalFrames - usedFrames;
        size_t frameSize = memManager->getFrameSize();
        
        int activeProcesses = static_cast<int>(runningProcesses.size());
        int inactiveProcesses = 0;
        
        for (const auto& process : allProcesses) {
            if (process->state == ProcessState::READY || 
                process->state == ProcessState::WAITING ||
                process->state == ProcessState::TERMINATED) {
                inactiveProcesses++;
            }
        }
        
        long long totalCpuTicks = memManager->getTotalCpuTicks();
        long long idleCpuTicks = memManager->getIdleCpuTicks();
        long long activeCpuTicks = memManager->getActiveCpuTicks();
        long long pageInCount = memManager->getPageInCount();
        long long pageOutCount = memManager->getPageOutCount();
        long long pageFaultCount = memManager->getPageFaultCount();
        
        double memoryUtilization = (totalMemoryKB > 0) ? 
            (static_cast<double>(usedMemoryKB) / totalMemoryKB * 100.0) : 0.0;
        double frameUtilization = (totalFrames > 0) ? 
            (static_cast<double>(usedFrames) / totalFrames * 100.0) : 0.0;
        double cpuUtilization = (totalCpuTicks > 0) ? 
            (static_cast<double>(activeCpuTicks) / totalCpuTicks * 100.0) : 0.0;
        
        std::cout << "\n";
        std::cout << "CSOPESY VMSTAT - Virtual Memory Statistics" << std::endl;
        std::cout << "=========================================================" << std::endl;
        
        std::cout << "\nMEMORY INFORMATION:" << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << std::setw(15) << totalMemoryKB << " K total memory" << std::endl;
        std::cout << std::setw(15) << usedMemoryKB << " K used memory (" 
                  << std::fixed << std::setprecision(1) << memoryUtilization << "%)" << std::endl;
        std::cout << std::setw(15) << freeMemoryKB << " K free memory" << std::endl;
        
        std::cout << "\nFRAME INFORMATION:" << std::endl;
        std::cout << "------------------" << std::endl;
        std::cout << std::setw(15) << totalFrames << " total frames" << std::endl;
        std::cout << std::setw(15) << usedFrames << " frames in use (" 
                  << std::fixed << std::setprecision(1) << frameUtilization << "%)" << std::endl;
        std::cout << std::setw(15) << freeFrames << " free frames" << std::endl;
        std::cout << std::setw(15) << frameSize << " bytes per frame" << std::endl;
        
        std::cout << "\nPROCESS INFORMATION:" << std::endl;
        std::cout << "--------------------" << std::endl;
        std::cout << std::setw(15) << activeProcesses << " active processes (running)" << std::endl;
        std::cout << std::setw(15) << inactiveProcesses << " inactive processes (ready/waiting/terminated)" << std::endl;
        std::cout << std::setw(15) << (activeProcesses + inactiveProcesses) << " total processes" << std::endl;
        
        std::cout << "\nCPU INFORMATION:" << std::endl;
        std::cout << "----------------" << std::endl;
        std::cout << std::setw(15) << totalCpuTicks << " total cpu ticks" << std::endl;
        std::cout << std::setw(15) << activeCpuTicks << " active cpu ticks (" 
                  << std::fixed << std::setprecision(1) << cpuUtilization << "%)" << std::endl;
        std::cout << std::setw(15) << idleCpuTicks << " idle cpu ticks" << std::endl;
        
        std::cout << "\nPAGING STATISTICS:" << std::endl;
        std::cout << "------------------" << std::endl;
        std::cout << std::setw(15) << pageFaultCount << " page faults" << std::endl;
        std::cout << std::setw(15) << pageInCount << " pages paged in (loaded from backing store)" << std::endl;
        std::cout << std::setw(15) << pageOutCount << " pages paged out (saved to backing store)" << std::endl;
        
        if (pageFaultCount > 0) {
            double pageFaultRate = static_cast<double>(pageInCount + pageOutCount) / pageFaultCount;
            std::cout << std::setw(15) << std::fixed << std::setprecision(2) << pageFaultRate 
                      << " avg page operations per fault" << std::endl;
        }
        
        std::cout << "=========================================================" << std::endl;
        std::cout << "\n";
    }
    
    void displayBackingStore() {
        std::cout << "\n";
        std::cout << "CSOPESY BACKING STORE CONTENTS" << std::endl;
        std::cout << "===============================================" << std::endl;
        
        std::ifstream backingStore("csopesy-backing-store.txt");
        if (!backingStore.is_open()) {
            std::cout << "Error: Cannot open backing store file." << std::endl;
            return;
        }
        
        std::string line;
        int lineCount = 0;
        while (std::getline(backingStore, line)) {
            std::cout << line << std::endl;
            lineCount++;
        }
        
        if (lineCount <= 3) {
            std::cout << "Backing store is currently empty (no page operations yet)." << std::endl;
        }
        
        backingStore.close();
        std::cout << "===============================================" << std::endl;
        std::cout << "Total entries: " << std::max(0, lineCount - 3) << std::endl;
        std::cout << "\n";
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
            } else if (args.size() >= 4 && args[1] == "-s") {
                std::string processName = args[2];
                
                size_t memorySize;
                try {
                    memorySize = std::stoull(args[3]);
                } catch (const std::exception& e) {
                    std::cout << "Invalid memory size format. Please enter a valid number." << std::endl;
                    return;
                }
                
                auto process = scheduler->findProcess(processName);
                if (!process) {
                    if (!scheduler->createProcess(processName, memorySize)) {
                        return; 
                    }
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
                            std::cout << "Memory Required: " << process->memoryRequired << " bytes" << std::endl;
                            std::cout << "Pages Required: " << process->pagesRequired << " pages" << std::endl;
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
                    if (process->memoryAccessViolation) {
                        std::cout << "Process " << processName << " shut down due to memory access violation error that occurred at " 
                                  << process->violationTimestamp << ". " << process->violationAddress << " invalid." << std::endl;
                    } else {
                        std::cout << "Process " << processName << " has already finished." << std::endl;
                    }
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
                        std::cout << "Memory Required: " << process->memoryRequired << " bytes" << std::endl;
                        std::cout << "Pages Required: " << process->pagesRequired << " pages" << std::endl;
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
            } else if (args.size() >= 4 && args[1] == "-c") {
                std::string processName = args[2];
                std::string memorySizeStr = args[3];
                
                int memorySize;
                try {
                    memorySize = std::stoi(memorySizeStr);
                } catch (const std::exception&) {
                    std::cout << "Invalid memory size: " << memorySizeStr << std::endl;
                    return;
                }
                
                if (!Utils::isValidMemorySize(memorySize)) {
                    std::cout << "Memory size must be between 64 and 65536 bytes and a power of 2." << std::endl;
                    return;
                }
                
                if (scheduler->findProcess(processName)) {
                    std::cout << "Process " << processName << " already exists." << std::endl;
                    return;
                }
                
                std::string instructionString;
                for (size_t i = 4; i < args.size(); ++i) {
                    if (i > 4) instructionString += " ";
                    instructionString += args[i];
                }
                
                if (instructionString.empty()) {
                    std::cout << "No instructions provided." << std::endl;
                    std::cout << "Usage: screen -c <process_name> <memory_size> <instructions>" << std::endl;
                    std::cout << "Example: screen -c process1 256 \"READ 0x10; WRITE 0x20; read 0x30\"" << std::endl;
                    return;
                }
                
                auto instructions = scheduler->parseInstructions(instructionString);
                if (instructions.empty()) {
                    std::cout << "Failed to parse instructions or no valid instructions found." << std::endl;
                    return;
                }
                
                if (instructions.size() > 50) {
                    std::cout << "Too many instructions. Maximum allowed is 50, provided: " << instructions.size() << std::endl;
                    return;
                }
                
                scheduler->createProcess(processName, memorySize, instructionString);
                std::cout << "Process " << processName << " created with " << instructions.size() << " custom instructions." << std::endl;
            } else if (args.size() >= 2 && args[1] == "-s") {
                std::cout << "Usage: screen -s <process_name> <memory_size>" << std::endl;
                std::cout << "Memory size must be between 64 and 65536 bytes and a power of 2." << std::endl;
                std::cout << "Example: screen -s process1 256" << std::endl;
            } else {
                std::cout << "Usage:" << std::endl;
                std::cout << "  screen -ls                                       List all processes" << std::endl;
                std::cout << "  screen -s <process_name> <memory_size>           Create new process screen session" << std::endl;
                std::cout << "  screen -c <process_name> <memory_size> <instr>   Create process with custom instructions" << std::endl;
                std::cout << "  screen -r <process_name>                         Resume existing process screen session" << std::endl;
                std::cout << "" << std::endl;
                std::cout << "Memory size must be between 64 and 65536 bytes and a power of 2." << std::endl;
                std::cout << "Instructions format: \"READ addr; WRITE addr; ...\" (max 50 instructions)" << std::endl;
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
                << "|  screen -s <name> <mem> - Create a process with specified memory allocation.   |\n"
                << "|  screen -r <name> - Resume an existing screen session if still running.        |\n"
                << "|       process-smi - Show process info inside screen.                            |\n"
                << "|       exit        - Exit the screen session.                                    |\n"
                << "|  screen -ls       - Show current CPU/process usage.                             |\n"
                << "|  scheduler-start  - Enable automatic dummy process generation.                  |\n"
                << "|  scheduler-test   - Start scheduler in test mode for performance testing.       |\n"
                << "|  scheduler-stop   - Disable automatic dummy process generation.                 |\n"
                << "|  report-util      - Save CPU utilization report to file.                        |\n"
                << "|  process-smi      - Show memory usage and running processes summary.            |\n"
                << "|  vmstat           - Show detailed memory and CPU statistics.                    |\n"
                << "|  backing-store    - Display contents of the backing store file.                 |\n"
                << "|  clear            - Clear the screen.                                           |\n"
                << "|  exit             - Exit the emulator.                                          |\n"
                << "+---------------------------------------------------------------------------------+\n"
                << "| NOTE: Memory size for 'screen -s' must be 64-65536 bytes and a power of 2.     |\n"
                << "+---------------------------------------------------------------------------------+\n";
        };
        
        commands["clear"] = [this](const std::vector<std::string>& args) {
            displayHeader();
        };
        
        commands["process-smi"] = [this](const std::vector<std::string>& args) {
            if (!initialized) {
                std::cout << "Please initialize the system first." << std::endl;
                return;
            }
            
            displayProcessSmi();
        };
        
        commands["vmstat"] = [this](const std::vector<std::string>& args) {
            if (!initialized) {
                std::cout << "Please initialize the system first." << std::endl;
                return;
            }
            
            displayVmstat();
        };
        
        commands["backing-store"] = [this](const std::vector<std::string>& args) {
            if (!initialized) {
                std::cout << "Please initialize the system first." << std::endl;
                return;
            }
            
            displayBackingStore();
        };
        
        commands["backing-store"] = [this](const std::vector<std::string>& args) {
            if (!initialized) {
                std::cout << "Please initialize the system first." << std::endl;
                return;
            }
            
            std::ifstream backingStore("csopesy-backing-store.txt");
            if (!backingStore.is_open()) {
                std::cout << "Backing store file not found." << std::endl;
                return;
            }
            
            std::cout << "\n=== BACKING STORE CONTENTS ===" << std::endl;
            std::string line;
            while (std::getline(backingStore, line)) {
                std::cout << line << std::endl;
            }
            std::cout << "==============================\n" << std::endl;
            backingStore.close();
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
