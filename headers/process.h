#ifndef PROCESS_NEW_H
#define PROCESS_NEW_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <atomic>

struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
    std::vector<std::string> sessionLog;
    std::vector<std::string> processLogs;
};

struct schedConfig {
    int numCores = 4;
    std::string algorithm = "fcfs";
    size_t quantumCycles = 5;
    size_t batchProcFreq = 1000;
    size_t minIns = 1000;
    size_t maxIns = 2000;
    size_t delayPerExec = 100;
    size_t maxOverallMem = 1024;
    size_t memPerFrame = 16;
    size_t memPerProc = 64;
};

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

struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
    int coreAssigned;
    std::string startTimestamp;
    std::string finishTimestamp;
    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    int memoryAddress = -1;
    uint8_t sleepRemaining = 0;
    int quantumRemaining = 0;
    bool isPreempted = false;
};

extern std::atomic<int> soloProcessCount;

bool readConfigFile(std::string filePath, schedConfig* config);
std::string getCurrentTimestamp();
void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out, int nestLevel = 0);
std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel = 0);

#endif // PROCESS_NEW_H
