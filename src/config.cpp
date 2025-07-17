#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

bool readConfigFile(std::string filePath, schedConfig* config) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error opening config file\n";
        return false;
    }
    std::string fileLine;
    while (std::getline(file, fileLine)) {
        std::istringstream iss(fileLine);
        std::string attribute;
        if (!(iss >> attribute)) continue;

        if (attribute == "scheduler") {
            std::string schedValue;
            if (iss >> std::quoted(schedValue)) {
                config->algorithm = schedValue;
            }
        } else {
            std::string configVals;
            if (!(iss >> configVals)) continue;
            if (attribute == "num-cpu") {
                config->numCores = std::stoi(configVals);
            } else if (attribute == "quantum-cycles") {
                config->quantumCycles = std::stoul(configVals);
            } else if (attribute == "batch-process-freq") {
                config->batchProcFreq = std::stoul(configVals);
            } else if (attribute == "min-ins") {
                config->minIns = std::stoul(configVals);
            } else if (attribute == "max-ins") {
                config->maxIns = std::stoul(configVals);
            } else if (attribute == "delay-per-exec") {
                config->delayPerExec = std::stoul(configVals);
            } else if (attribute == "max-overall-mem") {
                config->maxOverallMem = std::stoull(configVals);
            } else if (attribute == "mem-per-frame") {
                config->memPerFrame = std::stoull(configVals);
            } else if (attribute == "mem-per-proc") {
                config->memPerProc = std::stoull(configVals);
            }
        }
    }
    return true;
}