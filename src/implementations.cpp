#include "main.cpp"

// Enhanced Process Implementation
Process::Process(const std::string& processName) 
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

void Process::generateInstructions(int count) {
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

std::string Process::executeNextInstruction() {
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

bool Process::isComplete() const {
    return pendingInstructions.empty() && executedInstructions >= totalInstructions;
}

void Process::updateMetrics() {
    int currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    if (state == ProcessState::TERMINATED) {
        turnaroundTime = currentTime - arrivalTime;
        waitingTime = turnaroundTime - burstTime;
        completionTimestamp = Utils::getCurrentTimestamp();
    }
}

std::string Process::getStateString() const {
    switch (state) {
        case ProcessState::NEW: return "NEW";
        case ProcessState::READY: return "READY";
        case ProcessState::RUNNING: return "RUNNING";
        case ProcessState::WAITING: return "WAITING";
        case ProcessState::TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

// Enhanced MemoryManager Implementation
MemoryManager::MemoryManager(int totalMem, int frameSize) 
    : totalMemory(totalMem), frameSize(frameSize) {
    int numFrames = totalMemory / frameSize;
    memoryMap.resize(numFrames, false); // false = free, true = allocated
}

bool MemoryManager::allocate(const std::string& pid, int size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    int framesNeeded = (size + frameSize - 1) / frameSize; // Ceiling division
    int totalFrames = memoryMap.size();
    
    int startFrame = -1;
    
    switch (strategy) {
        case AllocationStrategy::FIRST_FIT: {
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
            break;
        }
        case AllocationStrategy::BEST_FIT: {
            int bestSize = totalFrames + 1;
            int bestStart = -1;
            int currentStart = -1;
            int currentSize = 0;
            
            for (int i = 0; i <= totalFrames; ++i) {
                if (i < totalFrames && !memoryMap[i]) {
                    if (currentStart == -1) currentStart = i;
                    currentSize++;
                } else {
                    if (currentSize >= framesNeeded && currentSize < bestSize) {
                        bestSize = currentSize;
                        bestStart = currentStart;
                    }
                    currentStart = -1;
                    currentSize = 0;
                }
            }
            startFrame = bestStart;
            break;
        }
        case AllocationStrategy::WORST_FIT: {
            int worstSize = -1;
            int worstStart = -1;
            int currentStart = -1;
            int currentSize = 0;
            
            for (int i = 0; i <= totalFrames; ++i) {
                if (i < totalFrames && !memoryMap[i]) {
                    if (currentStart == -1) currentStart = i;
                    currentSize++;
                } else {
                    if (currentSize >= framesNeeded && currentSize > worstSize) {
                        worstSize = currentSize;
                        worstStart = currentStart;
                    }
                    currentStart = -1;
                    currentSize = 0;
                }
            }
            startFrame = worstStart;
            break;
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

bool MemoryManager::deallocate(const std::string& pid) {
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

void MemoryManager::displayMemoryMap() const {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    std::cout << "\n=== Memory Map ===" << std::endl;
    std::cout << "Total Memory: " << totalMemory << " KB" << std::endl;
    std::cout << "Frame Size: " << frameSize << " KB" << std::endl;
    std::cout << "Available: " << getAvailableMemory() << " KB" << std::endl;
    std::cout << "Fragmentation: " << std::fixed << std::setprecision(2) 
              << getFragmentation() << "%" << std::endl;
    
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

double MemoryManager::getFragmentation() const {
    int freeFrames = 0;
    int largestFreeBlock = 0;
    int currentFreeBlock = 0;
    
    for (bool allocated : memoryMap) {
        if (!allocated) {
            freeFrames++;
            currentFreeBlock++;
            largestFreeBlock = std::max(largestFreeBlock, currentFreeBlock);
        } else {
            currentFreeBlock = 0;
        }
    }
    
    if (freeFrames == 0) return 0.0;
    return (1.0 - static_cast<double>(largestFreeBlock) / freeFrames) * 100.0;
}

int MemoryManager::getAvailableMemory() const {
    int freeFrames = 0;
    for (bool allocated : memoryMap) {
        if (!allocated) freeFrames++;
    }
    return freeFrames * frameSize;
}

void MemoryManager::compact() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    // Simple compaction: move all allocated blocks to the beginning
    std::vector<bool> newMemoryMap(memoryMap.size(), false);
    std::map<std::string, std::pair<int, int>> newAllocations;
    
    int nextFreeFrame = 0;
    
    // Sort processes by current address for stable compaction
    std::vector<std::pair<std::string, std::pair<int, int>>> sortedProcesses(
        allocatedProcesses.begin(), allocatedProcesses.end());
    
    std::sort(sortedProcesses.begin(), sortedProcesses.end(),
        [](const auto& a, const auto& b) {
            return a.second.first < b.second.first;
        });
    
    for (const auto& [pid, allocation] : sortedProcesses) {
        int size = allocation.second;
        int framesNeeded = (size + frameSize - 1) / frameSize;
        
        // Allocate at next available position
        for (int i = 0; i < framesNeeded; ++i) {
            newMemoryMap[nextFreeFrame + i] = true;
        }
        
        newAllocations[pid] = {nextFreeFrame * frameSize, size};
        nextFreeFrame += framesNeeded;
    }
    
    memoryMap = std::move(newMemoryMap);
    allocatedProcesses = std::move(newAllocations);
    
    std::cout << "Memory compaction completed." << std::endl;
}

// Utility function implementations
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
