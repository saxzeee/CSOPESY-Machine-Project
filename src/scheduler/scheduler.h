#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../config/config.h"
#include "../process/process.h"
#include "../memory/memory_manager.h"
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
    std::unique_ptr<MemoryManager> memoryManager;

    std::vector<std::shared_ptr<Process>> allProcesses;
    std::vector<std::shared_ptr<Process>> readyQueue;
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
    bool createProcess(const std::string& name, size_t memorySize);
    bool createProcess(const std::string& name, size_t memorySize, const std::vector<std::string>& instructions);
    void displaySystemStatus() const;
    void displayProcesses() const;
    void generateReport(const std::string& filename) const;
    std::shared_ptr<Process> findProcess(const std::string& name) const;
    bool isSystemRunning() const;
    void ensureSchedulerStarted();
    void enableDummyProcessGeneration();
    void disableDummyProcessGeneration();
    bool isDummyGenerationEnabled() const;
    
    MemoryManager* getMemoryManager() const { return memoryManager.get(); }
    const SystemConfig* getConfig() const { return config.get(); }
};

#endif
