#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct SystemConfig {
    int numCpu = 4;
    std::string scheduler = "fcfs";
    int quantumCycles = 5;
    int batchProcessFreq = 1;
    int minInstructions = 1000;
    int maxInstructions = 2000;
    int delayPerExec = 100;
    
    bool loadFromFile(const std::string& filename);
    void display() const;
};

#endif
