#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ICommand {
    enum class InstrType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };
};

struct Instruction {
    ICommand::InstrType type;
    std::string msg;
    std::string printVar;
    std::string var1, var2, var3;
    uint16_t value = 0;
    uint8_t sleepTicks = 0;
    std::vector<Instruction> body;
    int repeats = 0;
};