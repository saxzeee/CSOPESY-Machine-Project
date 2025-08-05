#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <atomic>

enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
};

class Process {
public:
    std::string pid;
    std::string name;
    ProcessState state;
    int priority;
    int arrivalTime;
    int burstTime;
    int remainingTime;
    int executedInstructions;
    int totalInstructions;
    int coreAssignment;
    std::string creationTimestamp;
    std::string completionTimestamp;
    std::vector<std::string> instructionHistory;
    std::queue<std::string> pendingInstructions;
    
    int waitingTime = 0;
    int turnaroundTime = 0;
    int responseTime = -1;
    int sleepRemaining = 0;
    
    size_t allocatedMemory = 0;
    uint32_t baseAddress = 0;
    bool memoryViolationOccurred = false;
    std::string violationTimestamp;
    uint32_t violationAddress = 0;
    
    Process(const std::string& processName);
    Process(const std::string& processName, size_t memorySize);
    Process(const std::string& processName, size_t memorySize, const std::vector<std::string>& customInstructions);
    
    void generateInstructions(int count);
    void setCustomInstructions(const std::vector<std::string>& instructions);
    std::string executeNextInstruction();
    bool isComplete() const;
    void updateMetrics();
    std::string getStateString() const;
    void setMemoryAllocation(size_t memory);
    void handleMemoryViolation(uint32_t address);
    bool hasMemoryViolation() const;
    std::string getViolationInfo() const;
    ~Process();

private:
    std::map<std::string, uint16_t> variables;
    
    std::string processInstruction(const std::string& instruction);
    std::string processDeclare(const std::string& instruction);
    std::string processAdd(const std::string& instruction);
    std::string processSubtract(const std::string& instruction);
    std::string processPrint(const std::string& instruction);
    std::string processSleep(const std::string& instruction);
    std::string processFor(const std::string& instruction);
    std::string processRead(const std::string& instruction);
    std::string processWrite(const std::string& instruction);
    uint16_t getVariableOrValue(const std::string& token);
};

#endif
