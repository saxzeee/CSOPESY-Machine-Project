#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <vector>
#include <map>
#include <queue>
#include <string>
#include <mutex>
#include <fstream>

struct MemoryFrame {
    bool occupied = false;
    std::string processId;
    uint32_t virtualPageNumber = 0;
    std::vector<uint8_t> data;
    
    MemoryFrame(size_t frameSize) : data(frameSize, 0) {}
};

struct ProcessMemoryInfo {
    std::string processId;
    size_t allocatedMemory = 0;
    uint32_t baseAddress = 0;
    std::map<uint32_t, uint32_t> pageTable;
    std::map<std::string, uint16_t> symbolTable;
    bool memoryViolationOccurred = false;
    std::string violationTimestamp;
    uint32_t violationAddress = 0;
    size_t symbolTableUsed = 0;
};

class MemoryManager {
private:
    size_t maxOverallMemory;
    size_t memoryPerFrame;
    size_t totalFrames;
    std::vector<MemoryFrame> frameTable;
    std::queue<uint32_t> freeFrames;
    std::map<std::string, ProcessMemoryInfo> processMemoryMap;
    std::string backingStorePath = "logs/csopesy-backing-store.txt";
    std::mutex memoryMutex;
    
    size_t minMemoryPerProcess;
    size_t maxMemoryPerProcess;
    
    size_t totalCpuTicks = 0;
    size_t idleCpuTicks = 0;
    size_t activeCpuTicks = 0;
    size_t pagesPagedIn = 0;
    size_t pagesPagedOut = 0;
    
    uint32_t findVictimFrame();
    void evictPageToBackingStore(uint32_t frameNumber);
    bool loadPageFromBackingStore(uint32_t frameNumber, const std::string& processId, uint32_t virtualPageNumber);
    
public:
    MemoryManager(size_t maxMemory, size_t frameSize, size_t minMemPerProc, size_t maxMemPerProc);
    ~MemoryManager();
    
    bool allocateMemory(const std::string& processId, size_t requiredMemory);
    void deallocateMemory(const std::string& processId);
    bool handlePageFault(const std::string& processId, uint32_t virtualAddress);
    uint16_t readMemory(const std::string& processId, uint32_t address);
    bool writeMemory(const std::string& processId, uint32_t address, uint16_t value);
    
    bool declareVariable(const std::string& processId, const std::string& varName, uint16_t value);
    bool getVariable(const std::string& processId, const std::string& varName, uint16_t& value);
    bool setVariable(const std::string& processId, const std::string& varName, uint16_t value);
    
    void generateMemoryReport();
    void generateVmstatReport();
    
    size_t getTotalMemory() const { return maxOverallMemory; }
    size_t getUsedMemory() const;
    size_t getFreeMemory() const { return maxOverallMemory - getUsedMemory(); }
    
    size_t getMinMemoryPerProcess() const { return minMemoryPerProcess; }
    size_t getMaxMemoryPerProcess() const { return maxMemoryPerProcess; }
    bool isValidMemorySize(size_t size) const;
    
    double getCpuUtilization() const { 
        return totalCpuTicks > 0 ? (static_cast<double>(activeCpuTicks) / totalCpuTicks) * 100.0 : 0.0; 
    }
    
    void incrementCpuTicks() { totalCpuTicks++; activeCpuTicks++; }
    void incrementIdleTicks() { totalCpuTicks++; idleCpuTicks++; }
    
    bool hasMemoryViolation(const std::string& processId) const;
    std::string getViolationInfo(const std::string& processId) const;
    
    size_t getProcessCount() const { return processMemoryMap.size(); }
    std::vector<size_t> getAllocatedMemorySizes() const;
};

#endif
