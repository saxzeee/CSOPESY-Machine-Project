#include "../headers/memory_manager.h"
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
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!blocks[i].allocated && blocks[i].size >= size) {
            if (blocks[i].size > size) {
                MemoryBlock newBlock = {blocks[i].start + size, blocks[i].size - size, false, ""};
                blocks[i].size = size;
                blocks.insert(blocks.begin() + i + 1, newBlock);
            }
            blocks[i].allocated = true;
            blocks[i].owner = owner;
            return static_cast<int>(blocks[i].start);
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
    for (size_t i = 0; i + 1 < blocks.size(); ) {
        if (!blocks[i].allocated && !blocks[i+1].allocated) {
            blocks[i].size += blocks[i+1].size;
            blocks.erase(blocks.begin() + i + 1);
        } else {
            ++i;
        }
    }
}

void MemoryManager::printMemory() const {
    std::cout << "--- Memory Blocks ---\n";
    for (const auto& block : blocks) {
        std::cout << "[" << block.start << ", " << (block.start + block.size - 1) << "] Size: " << block.size
                  << (block.allocated ? (" Allocated to: " + block.owner) : " Free") << "\n";
    }
}
