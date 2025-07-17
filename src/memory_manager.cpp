#include "memory_manager.h"
#include <iostream>
#include <sstream>

MemoryManager::MemoryManager(size_t totalSize) {
    blocks.push_back({0, totalSize, false, ""});
}

int MemoryManager::countProcessesInMemory() const {
    int count = 0;
    for (const auto& block : blocks) {
        if (block.allocated && !block.owner.empty()) count++;
    }
    return count;
}

size_t MemoryManager::getExternalFragmentation() const {
    size_t total = 0;
    for (const auto& block : blocks) {
        if (!block.allocated) total += block.size;
    }
    return total;
}

std::string MemoryManager::asciiMemoryMap() const {
    std::ostringstream oss;
    size_t mem_end = blocks.empty() ? 0 : blocks.back().start + blocks.back().size;
    oss << "----end----- = " << mem_end << "\n";
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
    return oss.str();
}

int MemoryManager::allocate(size_t size, const std::string& owner) {
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
        if (!it->allocated && it->size >= size) {
            size_t start = it->start;
            if (it->size > size) {
                blocks.insert(it + 1, {start + size, it->size - size, false, ""});
            }
            it->size = size;
            it->allocated = true;
            it->owner = owner;
            return static_cast<int>(start);
        }
    }
    return -1;
}

void MemoryManager::free(const std::string& owner) {
    for (auto& block : blocks) {
        if (block.allocated && block.owner == owner) {
            block.allocated = false;
            block.owner = "";
        }
    }
    mergeFreeBlocks();
}

void MemoryManager::mergeFreeBlocks() {
    for (auto it = blocks.begin(); it != blocks.end();) {
        if (!it->allocated) {
            auto next = it + 1;
            while (next != blocks.end() && !next->allocated) {
                it->size += next->size;
                next = blocks.erase(next);
            }
        }
        ++it;
    }
}

void MemoryManager::printMemory() const {
    std::cout << "--- Memory Blocks ---\n";
    for (const auto& block : blocks) {
        std::cout << "[" << block.start << ", " << (block.start + block.size - 1) << "] Size: " << block.size
                  << (block.allocated ? (" Allocated to: " + block.owner) : " Free") << "\n";
    }
}