#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../core/config.h"
#include "../process/process.h"
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

class Scheduler {
private:
    std::unique_ptr<SystemConfig> config;
    
    std::vector<std::shared_ptr<Process>> allProcesses;
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> runningProcesses; 
    std::vector<std::shared_ptr<Process>> terminatedProcesses;
    
    std::vector<std::thread> coreWorkers;
    std::atomic<bool> isRunning{false};
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> dummyProcessGenerationEnabled{false};
    mutable std::mutex processMutex;
    std::condition_variable processCV;
    
    std::atomic<int> processCounter{1};
    std::chrono::high_resolution_clock::time_point systemStartTime;
    
    std::vector<int> coreQuantumCounters;
    
    void coreWorkerThread(int coreId);
    void processCreatorThread();
    void testModeProcessCreator();
    void handleProcessCompletion(std::shared_ptr<Process> process);
    
public:
    Scheduler(std::unique_ptr<SystemConfig> cfg);
    ~Scheduler();
    
    bool start();
    bool startTestMode();
    void stop();
    bool createProcess(const std::string& name = "");
    void displaySystemStatus() const;
    void displayProcesses() const;
    void generateReport(const std::string& filename) const;
    std::shared_ptr<Process> findProcess(const std::string& name) const;
    bool isSystemRunning() const;
    void ensureSchedulerStarted();
    void enableDummyProcessGeneration();
    void disableDummyProcessGeneration();
    bool isDummyGenerationEnabled() const;
};

#endif
