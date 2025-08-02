#include "config.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>

bool SystemConfig::loadFromFile(const std::string& filename) {
    std::cout << "Attempting to load config from: " << filename << std::endl;
    
    // Try multiple possible paths
    std::vector<std::string> paths = {
        filename,
        "./" + filename,
        "../" + filename,
        "../../" + filename
    };
    
    std::ifstream file;
    std::string actualPath;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            actualPath = path;
            std::cout << "Successfully opened config at: " << actualPath << std::endl;
            break;
        }
        file.clear();
    }
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        std::cerr << "Tried the following paths:" << std::endl;
        for (const auto& path : paths) {
            std::cerr << "  - " << path << std::endl;
        }
        std::cerr << "Please ensure config.txt exists in the project root directory." << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find(' ');
        if (pos == std::string::npos) {
            pos = line.find('=');
            if (pos == std::string::npos) continue;
        }
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            end = value.find_last_not_of(" \t");
            value = value.substr(start, end - start + 1);
        }
        
        if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length() - 2);
        }
        
        try {
            if (key == "num-cpu") numCpu = std::stoi(value);
            else if (key == "scheduler") scheduler = value;
            else if (key == "quantum-cycles") quantumCycles = std::stoi(value);
            else if (key == "batch-process-freq") batchProcessFreq = std::stoi(value);
            else if (key == "min-ins") minInstructions = std::stoi(value);
            else if (key == "max-ins") maxInstructions = std::stoi(value);
            else if (key == "delay-per-exec") delayPerExec = std::stoi(value);
            else if (key == "max-overall-mem") maxOverallMemory = std::stoull(value);
            else if (key == "mem-per-frame") memoryPerFrame = std::stoull(value);
            else if (key == "min-mem-per-proc") minMemoryPerProcess = std::stoull(value);
            else if (key == "max-mem-per-proc") maxMemoryPerProcess = std::stoull(value);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config line: " << line << std::endl;
        }
    }
    
    return true;
}

void SystemConfig::display() const {
    std::cout << "---- Scheduler Configuration ----" << std::endl;
    std::cout << "Number of CPU Cores   : " << numCpu << std::endl;
    std::cout << "Scheduling Algorithm  : " << scheduler << std::endl;
    std::cout << "Quantum Cycles        : " << quantumCycles << std::endl;
    std::cout << "Batch Process Freq    : " << batchProcessFreq << std::endl;
    std::cout << "Min Instructions      : " << minInstructions << std::endl;
    std::cout << "Max Instructions      : " << maxInstructions << std::endl;
    std::cout << "Delay per Execution   : " << delayPerExec << std::endl;
    std::cout << "Max Overall Memory    : " << maxOverallMemory << " bytes" << std::endl;
    std::cout << "Memory per Frame      : " << memoryPerFrame << " bytes" << std::endl;
    std::cout << "Min Memory per Process: " << minMemoryPerProcess << " bytes" << std::endl;
    std::cout << "Max Memory per Process: " << maxMemoryPerProcess << " bytes" << std::endl;
    std::cout << "----------------------------------" << std::endl;
}
