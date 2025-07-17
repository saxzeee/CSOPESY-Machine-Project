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
    size_t maxOverallMem;
    size_t memPerFrame;
    size_t minMemPerProc;
    size_t maxMemPerProc;
};

bool readConfigFile(std::string filePath, schedConfig* config);