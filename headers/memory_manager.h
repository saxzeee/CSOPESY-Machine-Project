#pragma once
#include <vector>
#include <string>
#include "memory_block.h"

class MemoryManager {
public:
    MemoryManager(size_t totalSize);
    int countProcessesInMemory() const;
    size_t getExternalFragmentation() const;
    std::string asciiMemoryMap() const;
    int allocate(size_t size, const std::string& owner);
    void free(const std::string& owner);
    void printMemory() const;
    void mergeFreeBlocks();
private:
    std::vector<MemoryBlock> blocks;
};