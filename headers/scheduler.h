#pragma once
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <deque>
#include <memory>
#include <atomic>
#include <atomic>
#include "Process.h"
#include "screen_session.h"
#include "memory_manager.h"
#include "Config.h"

class Scheduler {
public:
    Scheduler(const schedConfig& config);
    void writeMemoryReport(int quantumCycle);
    bool addProcess(Process& process, bool suppressError = false);
    const std::vector<Process>& getProcessList() const;
    const std::vector<int>& getFinishedProcesses() const;
    void startScheduler();
    bool isRunning() const;
    void runScheduler();
    void generateLog(const std::string& filename);
    void shutdown();
    void printMemory() const;
    void printConfig() const;
    void showScreenLS();
    void setScreenMap(std::map<std::string, ScreenSession>* screens);
    void freeProcessMemory(const std::string& name);
    std::mutex& getProcessMutex();
    std::vector<std::string>& getCoreToProcess();

    unsigned int numCores;
    std::string algorithm;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;
private:
    std::vector<Process> processList;
    std::vector<int> finishedProcesses;
    std::vector<std::string> coreToProcess;
    std::mutex processMutex;
    bool schedulerRunning = false;
    std::thread schedulerMain;
    std::map<std::string, ScreenSession>* screenSessions = nullptr;
    std::unique_ptr<MemoryManager> memoryManager;
    size_t memPerProc;
    size_t memPerFrame;
    size_t maxOverallMem;
    std::deque<Process> pendingQueue;

    std::atomic<int> quantumFinishCounter{0};
    std::atomic<int> quantumCycleNumber{0};

    void cpuWorker(int coreID);
    void cpuWorkerRoundRobin(int coreID);
    void scheduler();
};