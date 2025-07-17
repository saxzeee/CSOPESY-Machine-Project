#pragma once
#include <string>

struct MemoryBlock {
    size_t start;
    size_t size;
    bool allocated;
    std::string owner; 
};