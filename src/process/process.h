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
    
    Process(const std::string& processName);
    void generateInstructions(int count);
    std::string executeNextInstruction();
    bool isComplete() const;
    void updateMetrics();
    std::string getStateString() const;
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
    uint16_t getVariableOrValue(const std::string& token);
};

#endif
