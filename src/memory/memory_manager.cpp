#include "memory_manager.h"
#include "../utils/utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <fstream>

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
    
    size_t pagesNeeded = (requiredMemory + memoryPerFrame - 1) / memoryPerFrame;
    
    size_t totalVirtualMemoryAllocated = 0;
    for (const auto& pair : processMemoryMap) {
        totalVirtualMemoryAllocated += pair.second.allocatedMemory;
    }
    
    if (totalVirtualMemoryAllocated + requiredMemory > maxOverallMemory) {
        return false;
    }
    
    if (freeFrames.size() < pagesNeeded) {
        return false; 
    }
    
    ProcessMemoryInfo memInfo;
    memInfo.processId = processId;
    memInfo.allocatedMemory = requiredMemory;
    memInfo.baseAddress = processMemoryMap.size() * 0x10000;
    
    for (size_t i = 0; i < pagesNeeded; ++i) {
        if (freeFrames.empty()) {
            for (auto& pagePair : memInfo.pageTable) {
                uint32_t frameNumber = pagePair.second;
                frameTable[frameNumber].occupied = false;
                frameTable[frameNumber].processId.clear();
                freeFrames.push(frameNumber);
            }
            return false;
        }
        
        uint32_t frameNumber = freeFrames.front();
        freeFrames.pop();
        
        frameTable[frameNumber].occupied = true;
        frameTable[frameNumber].processId = processId;
        frameTable[frameNumber].virtualPageNumber = i;
        
        memInfo.pageTable[i] = frameNumber;
    }
    
    processMemoryMap[processId] = memInfo;
    
    return true;
}

void MemoryManager::deallocateMemory(const std::string& processId) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return;
    
    for (auto& pagePair : it->second.pageTable) {
        uint32_t frameNumber = pagePair.second;
        if (frameNumber < totalFrames) {
            frameTable[frameNumber].occupied = false;
            frameTable[frameNumber].processId.clear();
            freeFrames.push(frameNumber);
        }
    }
    
    processMemoryMap.erase(it);
}

bool MemoryManager::handlePageFault(const std::string& processId, uint32_t virtualAddress) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processMemoryMap.find(processId);
    if (it == processMemoryMap.end()) return false;
    
    uint32_t pageNumber = virtualAddress / memoryPerFrame;
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
    
    it->second.pageTable[pageNumber] = frameNumber;
    pagesPagedIn++;
    
    return true;
}

uint32_t MemoryManager::findVictimFrame() {
    for (size_t i = 0; i < totalFrames; ++i) {
        if (frameTable[i].occupied) {
            return i;
        }
    }
    return 0;
}

void MemoryManager::evictPageToBackingStore(uint32_t frameNumber) {
    if (frameNumber >= totalFrames || !frameTable[frameNumber].occupied) return;
    
    std::ofstream backingStore(backingStorePath, std::ios::app);
    if (backingStore.is_open()) {
        backingStore << "EVICTED: Process=" << frameTable[frameNumber].processId 
                   << " Page=" << frameTable[frameNumber].virtualPageNumber
                   << " Frame=" << frameNumber << "\n";
        
        for (size_t i = 0; i < frameTable[frameNumber].data.size(); ++i) {
            backingStore << std::hex << static_cast<int>(frameTable[frameNumber].data[i]) << " ";
            if ((i + 1) % 16 == 0) backingStore << "\n";
        }
        backingStore << "\n";
        backingStore.close();
    }
    
    auto it = processMemoryMap.find(frameTable[frameNumber].processId);
    if (it != processMemoryMap.end()) {
        it->second.pageTable.erase(frameTable[frameNumber].virtualPageNumber);
    }
    
    frameTable[frameNumber].occupied = false;
    frameTable[frameNumber].processId.clear();
    pagesPagedOut++;
}

bool MemoryManager::loadPageFromBackingStore(uint32_t frameNumber, const std::string& processId, uint32_t virtualPageNumber) {
    std::ifstream backingStore(backingStorePath);
    if (!backingStore.is_open()) return false;
    
    std::string line;
    bool found = false;
    std::vector<uint8_t> pageData;
    
    while (std::getline(backingStore, line)) {
        if (line.find("EVICTED: Process=" + processId + " Page=" + std::to_string(virtualPageNumber)) != std::string::npos) {
            found = true;
            continue;
        }
        
        if (found && !line.empty() && line != "EVICTED") {
            std::istringstream iss(line);
            std::string hexByte;
            while (iss >> hexByte) {
                try {
                    uint8_t byte = static_cast<uint8_t>(std::stoul(hexByte, nullptr, 16));
                    pageData.push_back(byte);
                } catch (const std::exception&) {
                    break;
                }
            }
            
            if (pageData.size() >= memoryPerFrame) break;
        }
    }
    
    if (found && pageData.size() >= memoryPerFrame) {
        std::copy(pageData.begin(), pageData.begin() + memoryPerFrame, frameTable[frameNumber].data.begin());
        return true;
    }
    
    return false;
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
    if (pageIt == it->second.pageTable.end()) {
        if (!const_cast<MemoryManager*>(this)->handlePageFault(processId, address)) {
            return 0;
        }
        pageIt = it->second.pageTable.find(pageNumber);
    }
    
    uint32_t frameNumber = pageIt->second;
    if (frameNumber >= totalFrames || offset + 1 >= memoryPerFrame) return 0;
    
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
    if (pageIt == it->second.pageTable.end()) {
        if (!handlePageFault(processId, address)) {
            return false;
        }
        pageIt = it->second.pageTable.find(pageNumber);
    }
    
    uint32_t frameNumber = pageIt->second;
    if (frameNumber >= totalFrames || offset + 1 >= memoryPerFrame) return false;
    
    frameTable[frameNumber].data[offset] = value & 0xFF;
    frameTable[frameNumber].data[offset + 1] = (value >> 8) & 0xFF;
    
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

void MemoryManager::generateMemoryReport() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    size_t physicalMemoryUsed = getUsedMemory();
    size_t freeMemory = maxOverallMemory - physicalMemoryUsed;
    double cpuUtil = totalCpuTicks > 0 ? (static_cast<double>(activeCpuTicks) / totalCpuTicks) * 100.0 : 0.0;
    
    std::cout << "==========================================" << std::endl;
    std::cout << "| CSOPESY Process and Memory Monitor     |" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "CPU-Util: " << std::fixed << std::setprecision(1) << cpuUtil << "%" << std::endl;
    std::cout << "Memory: " << physicalMemoryUsed << " / " << maxOverallMemory << " bytes" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Running processes and memory usage:" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    for (const auto& pair : processMemoryMap) {
        std::cout << std::left << std::setw(20) << pair.first 
                  << std::right << std::setw(10) << pair.second.allocatedMemory << " bytes" << std::endl;
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
