#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "instruction.h"

struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
    std::string startTimestamp;
    std::string finishTimestamp;
    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    int coreAssigned = -1;
    int sleepRemaining = 0;
    size_t memoryRequired = 0;
    int memoryAddress = -1;
};