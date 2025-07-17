#pragma once
#include <cstdint>
#include <string>

struct schedConfig {
    unsigned int numCores;
    std::string algorithm;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;
    size_t maxOverallMem = 1024;
    size_t memPerFrame = 0;
    size_t memPerProc = 64;
};

bool readConfigFile(std::string filePath, schedConfig* config);