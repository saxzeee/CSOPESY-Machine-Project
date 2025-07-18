#ifndef SCHEDULER_NEW_H
#define SCHEDULER_NEW_H

#include "memory_manager.h"
#include "process.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <deque>
#include <memory>
#include <atomic>
#include <map>

class Scheduler {
public:
    int numCores;
    std::string algorithm;
    size_t quantumCycles;
    size_t batchProcFreq;
    size_t minIns;
    size_t maxIns;
    size_t delayPerExec;
    size_t maxOverallMem;
    size_t memPerFrame;
    size_t memPerProc;

private:
    std::vector<Process> processList;
    std::deque<Process> pendingQueue;
    std::deque<Process> readyQueue;
    std::vector<std::string> coreToProcess;
    std::vector<std::thread> cpuThreads;
    std::thread schedulerMain;
    std::mutex processMutex;
    std::atomic<bool> schedulerRunning{false};
    std::unique_ptr<MemoryManager> memoryManager;
    std::map<std::string, ScreenSession>* screenSessions = nullptr;

public:
    explicit Scheduler(const schedConfig& config);
    ~Scheduler();

    bool addProcess(const Process& proc, bool suppressError = false);
    void freeProcessMemory(const std::string& processName);
    const std::vector<Process>& getProcessList() const { return processList; }
    
    void startScheduler();
    void scheduler();
    void runScheduler();
    void shutdown();
    
    void generateLog(const std::string& filename);
    void printMemory() const;
    void printConfig() const;
    void showScreenLS();
    void setScreenMap(std::map<std::string, ScreenSession>* screens);
    
    bool isRunning() const { return schedulerRunning; }
    std::mutex& getProcessMutex() { return processMutex; }
    std::vector<std::string>& getCoreToProcess() { return coreToProcess; }

private:
    void processGeneratorLoop();
    void cpuWorker(int coreId);
    void processSchedulingAlgorithm();
    int findProcessIndex(const std::string& name) const;
};

#endif // SCHEDULER_NEW_H
