#include "memory_manager.h"
#include "../utils/utils.h"
#include "../process/process.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <fstream>
#include <filesystem>

MemoryManager::MemoryManager(size_t maxMemory, size_t frameSize, size_t minMemPerProc, size_t maxMemPerProc) 
    : maxOverallMemory(maxMemory), memoryPerFrame(frameSize), 
      minMemoryPerProcess(minMemPerProc), maxMemoryPerProcess(maxMemPerProc) {
    totalFrames = maxMemory / frameSize;
    frameTable.reserve(totalFrames);
    
    for (size_t i = 0; i < totalFrames; ++i) {
        frameTable.emplace_back(frameSize);
        freeFrames.push(i);
    }
    
    std::ofstream backingStore(backingStorePath, std::ios::trunc);
    if (backingStore.is_open()) {
        backingStore << "CSOPESY Backing Store - Initialized\n";
        backingStore.close();
    }
}

MemoryManager::~MemoryManager() {
}

bool MemoryManager::isValidMemorySize(size_t size) const {
    if (size < minMemoryPerProcess || size > maxMemoryPerProcess) return false;
    if (size < 64 || size > 65536) return false;
    
    return (size & (size - 1)) == 0;
}

bool MemoryManager::allocateMemory(const std::string& processId, size_t requiredMemory) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if (!isValidMemorySize(requiredMemory)) {
        return false;
    }
    
    if (processMemoryMap.find(processId) != processMemoryMap.end()) {
        return false;
    }
    
    ProcessMemoryInfo memInfo;
    memInfo.processId = processId;
    memInfo.allocatedMemory = requiredMemory;
    memInfo.baseAddress = processMemoryMap.size() * 0x10000;
    
    processMemoryMap[processId] = memInfo;
    initializeProcessPages(processId, requiredMemory);
    
    handlePageFaultInternal(processId, 0);
    
    return true;
}

void MemoryManager::initializeProcessPages(const std::string& processId, size_t memorySize) {
    size_t pagesNeeded = (memorySize + memoryPerFrame - 1) / memoryPerFrame;
    
    auto& processInfo = processMemoryMap[processId];
    
    for (uint32_t i = 0; i < pagesNeeded; ++i) {
        PageTableEntry entry;
        entry.valid = false;
        entry.frameNumber = 0;
        entry.referenced = false;
        entry.modified = false;
        
        processInfo.pageTable[i] = entry;
        processInfo.validPages.insert(i);
        
        createInitialBackingStoreEntry(processId, i);
    }
}

void MemoryManager::deallocateMemory(const std::string& processId) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return;
    
    for (auto& pagePair : it->second.pageTable) {
        uint32_t pageNumber = pagePair.first;
        PageTableEntry& entry = pagePair.second;
        
        if (entry.valid && entry.frameNumber < totalFrames) {
            frameTable[entry.frameNumber].occupied = false;
            frameTable[entry.frameNumber].processId.clear();
            freeFrames.push(entry.frameNumber);
        }
        
        removeBackingStoreEntry(processId, pageNumber);
    }
    
    processMemoryMap.erase(it);
}

bool MemoryManager::handlePageFault(const std::string& processId, uint32_t virtualAddress) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    return handlePageFaultInternal(processId, virtualAddress);
}

bool MemoryManager::handlePageFaultInternal(const std::string& processId, uint32_t virtualAddress) {
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    uint32_t pageNumber = virtualAddress / memoryPerFrame;
    
    if (it->second.validPages.find(pageNumber) == it->second.validPages.end()) {
        return false;
    }
    
    uint32_t frameNumber;
    
    if (freeFrames.empty()) {
        frameNumber = findVictimFrame();
        evictPageToBackingStore(frameNumber);
    } else {
        frameNumber = freeFrames.front();
        freeFrames.pop();
    }
    
    if (!loadPageFromBackingStore(frameNumber, processId, pageNumber)) {
        frameTable[frameNumber].data.assign(memoryPerFrame, 0);
    }
    
    frameTable[frameNumber].occupied = true;
    frameTable[frameNumber].processId = processId;
    frameTable[frameNumber].virtualPageNumber = pageNumber;
    frameTable[frameNumber].lastAccessTime = currentTime;
    
    PageTableEntry& entry = it->second.pageTable[pageNumber];
    entry.valid = true;
    entry.frameNumber = frameNumber;
    entry.referenced = true;
    
    pagesPagedIn++;
    
    return true;
}

uint32_t MemoryManager::findVictimFrame() {
    size_t oldestTime = currentTime;
    uint32_t victimFrame = 0;
    
    for (size_t i = 0; i < totalFrames; ++i) {
        if (frameTable[i].occupied && frameTable[i].lastAccessTime < oldestTime) {
            oldestTime = frameTable[i].lastAccessTime;
            victimFrame = i;
        }
    }
    
    return victimFrame;
}

void MemoryManager::evictPageToBackingStore(uint32_t frameNumber) {
    if (frameNumber >= totalFrames || !frameTable[frameNumber].occupied) return;
    
    std::string processId = frameTable[frameNumber].processId;
    uint32_t pageNumber = frameTable[frameNumber].virtualPageNumber;
    
    writePageToBackingStore(processId, pageNumber, frameTable[frameNumber].data);
    
    auto it = processMemoryMap.find(processId);
    if (it != processMemoryMap.end()) {
        auto pageIt = it->second.pageTable.find(pageNumber);
        if (pageIt != it->second.pageTable.end()) {
            pageIt->second.valid = false;
            pageIt->second.frameNumber = 0;
        }
    }
    
    frameTable[frameNumber].occupied = false;
    frameTable[frameNumber].processId.clear();
    pagesPagedOut++;
}

bool MemoryManager::loadPageFromBackingStore(uint32_t frameNumber, const std::string& processId, uint32_t virtualPageNumber) {
    return readPageFromBackingStore(processId, virtualPageNumber, frameTable[frameNumber].data);
}

void MemoryManager::createInitialBackingStoreEntry(const std::string& processId, uint32_t pageNumber) {
    std::vector<uint8_t> initialData(memoryPerFrame, 0);
    writePageToBackingStore(processId, pageNumber, initialData);
}

void MemoryManager::writePageToBackingStore(const std::string& processId, uint32_t pageNumber, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(backingStoreMutex);
    
    std::vector<std::string> lines;
    std::ifstream inputFile(backingStorePath);
    std::string line;
    bool found = false;
    std::string pageId = "PROCESS=" + processId + " PAGE=" + std::to_string(pageNumber);
    
    while (std::getline(inputFile, line)) {
        if (line.find(pageId) != std::string::npos) {
            found = true;
            lines.push_back(pageId);
            
            std::ostringstream dataStream;
            for (size_t i = 0; i < data.size(); ++i) {
                dataStream << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
                if ((i + 1) % 16 == 0) dataStream << "\n";
                else dataStream << " ";
            }
            if (data.size() % 16 != 0) dataStream << "\n";
            dataStream << "END_PAGE\n";
            
            lines.push_back(dataStream.str());
            
            while (std::getline(inputFile, line) && line != "END_PAGE") {
            }
        } else {
            lines.push_back(line);
        }
    }
    inputFile.close();
    
    if (!found) {
        lines.push_back(pageId);
        
        std::ostringstream dataStream;
        for (size_t i = 0; i < data.size(); ++i) {
            dataStream << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
            if ((i + 1) % 16 == 0) dataStream << "\n";
            else dataStream << " ";
        }
        if (data.size() % 16 != 0) dataStream << "\n";
        dataStream << "END_PAGE";
        
        lines.push_back(dataStream.str());
    }
    
    std::ofstream outputFile(backingStorePath);
    for (const auto& outputLine : lines) {
        outputFile << outputLine << "\n";
    }
    outputFile.close();
}

bool MemoryManager::readPageFromBackingStore(const std::string& processId, uint32_t pageNumber, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(backingStoreMutex);
    
    std::ifstream inputFile(backingStorePath);
    if (!inputFile.is_open()) return false;
    
    std::string line;
    std::string pageId = "PROCESS=" + processId + " PAGE=" + std::to_string(pageNumber);
    bool found = false;
    
    while (std::getline(inputFile, line)) {
        if (line.find(pageId) != std::string::npos) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        inputFile.close();
        return false;
    }
    
    data.clear();
    data.resize(memoryPerFrame, 0);
    size_t dataIndex = 0;
    
    while (std::getline(inputFile, line) && line != "END_PAGE" && dataIndex < memoryPerFrame) {
        std::istringstream iss(line);
        std::string hexByte;
        
        while (iss >> hexByte && dataIndex < memoryPerFrame) {
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoul(hexByte, nullptr, 16));
                data[dataIndex++] = byte;
            } catch (const std::exception&) {
                break;
            }
        }
    }
    
    inputFile.close();
    return dataIndex > 0;
}

void MemoryManager::removeBackingStoreEntry(const std::string& processId, uint32_t pageNumber) {
    std::lock_guard<std::mutex> lock(backingStoreMutex);
    
    std::vector<std::string> lines;
    std::ifstream inputFile(backingStorePath);
    std::string line;
    std::string pageId = "PROCESS=" + processId + " PAGE=" + std::to_string(pageNumber);
    
    while (std::getline(inputFile, line)) {
        if (line.find(pageId) != std::string::npos) {
            while (std::getline(inputFile, line) && line != "END_PAGE") {
            }
        } else {
            lines.push_back(line);
        }
    }
    inputFile.close();
    
    std::ofstream outputFile(backingStorePath);
    for (const auto& outputLine : lines) {
        outputFile << outputLine << "\n";
    }
    outputFile.close();
}

uint16_t MemoryManager::readMemory(const std::string& processId, uint32_t address) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return 0;
    
    if (address >= it->second.allocatedMemory) {
        it->second.memoryViolationOccurred = true;
        it->second.violationAddress = address;
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        it->second.violationTimestamp = oss.str();
        return 0;
    }
    
    uint32_t pageNumber = address / memoryPerFrame;
    uint32_t offset = address % memoryPerFrame;
    
    auto pageIt = it->second.pageTable.find(pageNumber);
    if (pageIt == it->second.pageTable.end() || !pageIt->second.valid) {
        if (!const_cast<MemoryManager*>(this)->handlePageFaultInternal(processId, address)) {
            return 0;
        }
        pageIt = it->second.pageTable.find(pageNumber);
    }
    
    uint32_t frameNumber = pageIt->second.frameNumber;
    if (frameNumber >= totalFrames || offset + 1 >= memoryPerFrame) return 0;
    
    frameTable[frameNumber].lastAccessTime = currentTime;
    pageIt->second.referenced = true;
    
    uint16_t value = frameTable[frameNumber].data[offset] | 
                    (static_cast<uint16_t>(frameTable[frameNumber].data[offset + 1]) << 8);
    return value;
}

bool MemoryManager::writeMemory(const std::string& processId, uint32_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    if (address >= it->second.allocatedMemory) {
        it->second.memoryViolationOccurred = true;
        it->second.violationAddress = address;
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        it->second.violationTimestamp = oss.str();
        return false;
    }
    
    uint32_t pageNumber = address / memoryPerFrame;
    uint32_t offset = address % memoryPerFrame;
    
    auto pageIt = it->second.pageTable.find(pageNumber);
    if (pageIt == it->second.pageTable.end() || !pageIt->second.valid) {
        if (!handlePageFaultInternal(processId, address)) {
            return false;
        }
        pageIt = it->second.pageTable.find(pageNumber);
    }
    
    uint32_t frameNumber = pageIt->second.frameNumber;
    if (frameNumber >= totalFrames || offset + 1 >= memoryPerFrame) return false;
    
    frameTable[frameNumber].lastAccessTime = currentTime;
    pageIt->second.referenced = true;
    pageIt->second.modified = true;
    
    frameTable[frameNumber].data[offset] = value & 0xFF;
    frameTable[frameNumber].data[offset + 1] = (value >> 8) & 0xFF;
    
    return true;
}

bool MemoryManager::accessMemory(const std::string& processId, uint32_t address) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    if (address >= it->second.allocatedMemory) {
        it->second.memoryViolationOccurred = true;
        it->second.violationAddress = address;
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        it->second.violationTimestamp = oss.str();
        return false;
    }
    
    uint32_t pageNumber = address / memoryPerFrame;
    
    auto pageIt = it->second.pageTable.find(pageNumber);
    if (pageIt == it->second.pageTable.end() || !pageIt->second.valid) {
        if (!handlePageFaultInternal(processId, address)) {
            return false;
        }
        pageIt = it->second.pageTable.find(pageNumber);
    }
    
    uint32_t frameNumber = pageIt->second.frameNumber;
    if (frameNumber >= totalFrames) return false;
    
    frameTable[frameNumber].lastAccessTime = currentTime;
    pageIt->second.referenced = true;
    
    return true;
}

bool MemoryManager::declareVariable(const std::string& processId, const std::string& varName, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    if (it->second.symbolTableUsed >= 64) {
        return false;
    }
    
    if (it->second.symbolTable.find(varName) != it->second.symbolTable.end()) {
        it->second.symbolTable[varName] = value;
        return true;
    }
    
    it->second.symbolTable[varName] = value;
    it->second.symbolTableUsed += 2;
    
    return true;
}

bool MemoryManager::getVariable(const std::string& processId, const std::string& varName, uint16_t& value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    auto varIt = it->second.symbolTable.find(varName);
    if (varIt == it->second.symbolTable.end()) return false;
    
    value = varIt->second;
    return true;
}

bool MemoryManager::setVariable(const std::string& processId, const std::string& varName, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    auto varIt = it->second.symbolTable.find(varName);
    if (varIt == it->second.symbolTable.end()) return false;
    
    varIt->second = value;
    return true;
}

size_t MemoryManager::getUsedMemory() const {
    size_t usedFrames = 0;
    for (const auto& frame : frameTable) {
        if (frame.occupied) {
            usedFrames++;
        }
    }
    return usedFrames * memoryPerFrame;
}

size_t MemoryManager::getVirtualMemoryUsed() const {
    size_t totalVirtualMemory = 0;
    for (const auto& pair : processMemoryMap) {
        totalVirtualMemory += pair.second.allocatedMemory;
    }
    return totalVirtualMemory;
}

// void MemoryManager::generateMemoryReport() {
//     std::lock_guard<std::mutex> lock(memoryMutex);
    
//     size_t physicalMemoryUsed = getUsedMemory();
//     size_t freeMemory = maxOverallMemory - physicalMemoryUsed;
//     double cpuUtil = totalCpuTicks > 0 ? (static_cast<double>(activeCpuTicks) / totalCpuTicks) * 100.0 : 0.0;
    
//     std::cout << "==========================================" << std::endl;
//     std::cout << "| CSOPESY Process and Memory Monitor     |" << std::endl;
//     std::cout << "==========================================" << std::endl;
//     std::cout << "CPU-Util: " << std::fixed << std::setprecision(1) << cpuUtil << "%" << std::endl;
//     std::cout << "Memory: " << physicalMemoryUsed << " / " << maxOverallMemory << " bytes" << std::endl;
//     std::cout << "==========================================" << std::endl;
//     std::cout << "Running processes and memory usage:" << std::endl;
//     std::cout << "------------------------------------------" << std::endl;
    
//     for (const auto& pair : processMemoryMap) {
//         std::cout << std::left << std::setw(20) << pair.first 
//                   << std::right << std::setw(10) << pair.second.allocatedMemory << " bytes" << std::endl;
//     }
    
//     std::cout << "------------------------------------------" << std::endl;
// }

void MemoryManager::generateMemoryReport(const std::vector<std::shared_ptr<Process>>& runningProcesses, int numCpu) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    size_t virtualMemoryUsed = getVirtualMemoryUsed();
    size_t freeVirtualMemory = maxOverallMemory - virtualMemoryUsed;
    
    int busyCores = 0;
    for (const auto& process : runningProcesses) {
        if (process != nullptr) busyCores++;
    }
    
    int totalCores = numCpu;
    int coresAvailable = totalCores - busyCores;
    double cpuUtilization = (static_cast<double>(busyCores) / totalCores) * 100.0;
    double memoryUtilization = (static_cast<double>(virtualMemoryUsed) / maxOverallMemory) * 100.0;
    
    std::cout << "==========================================" << std::endl;
    std::cout << "| CSOPESY Process and Memory Monitor     |" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "CPU-Util: " << std::fixed << std::setprecision(1) << cpuUtilization << "%" << std::endl;
    std::cout << "Memory-Util: " << std::fixed << std::setprecision(1) << memoryUtilization << "%" << std::endl;
    std::cout << "Memory: " << virtualMemoryUsed << " / " << maxOverallMemory << " bytes" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Running processes and memory usage:" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    for (const auto& process : runningProcesses) {
        if (process != nullptr) {
            auto it = processMemoryMap.find(process->pid);
            if (it != processMemoryMap.end()) {
                std::cout << std::left << std::setw(20) << process->pid 
                          << std::right << std::setw(10) << it->second.allocatedMemory << " bytes" << std::endl;
            }
        }
    }
    
    std::cout << "------------------------------------------" << std::endl;
}

void MemoryManager::generateVmstatReport() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    size_t physicalMemoryUsed = getUsedMemory();
    size_t freeMemory = maxOverallMemory - physicalMemoryUsed;
    
    std::cout << "Total memory: " << maxOverallMemory << " bytes" << std::endl;
    std::cout << "Used memory: " << physicalMemoryUsed << " bytes" << std::endl;
    std::cout << "Free memory: " << freeMemory << " bytes" << std::endl;
    std::cout << "Idle CPU ticks: " << idleCpuTicks << std::endl;
    std::cout << "Active CPU ticks: " << activeCpuTicks << std::endl;
    std::cout << "Total CPU ticks: " << totalCpuTicks << std::endl;
    std::cout << "Num paged in: " << pagesPagedIn << std::endl;
    std::cout << "Num paged out: " << pagesPagedOut << std::endl;
}

bool MemoryManager::hasMemoryViolation(const std::string& processId) const {
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    return it->second.memoryViolationOccurred;
}

std::string MemoryManager::getViolationInfo(const std::string& processId) const {
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end() || !it->second.memoryViolationOccurred) return "";
    
    std::ostringstream oss;
    oss << "Process " << processId << " shut down due to memory access violation error that occurred at " 
        << it->second.violationTimestamp << ". 0x" << std::hex << it->second.violationAddress << " invalid.";
    return oss.str();
}

std::vector<size_t> MemoryManager::getAllocatedMemorySizes() const {
    std::vector<size_t> sizes;

    for (const auto& pair : processMemoryMap) {
        sizes.push_back(pair.second.allocatedMemory);
    }
    return sizes;
}
