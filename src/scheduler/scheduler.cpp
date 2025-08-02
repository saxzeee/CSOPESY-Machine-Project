#include "scheduler.h"
#include "../utils/utils.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <map>

Scheduler::Scheduler(std::unique_ptr<SystemConfig> cfg) 
    : config(std::move(cfg)) {
    
    runningProcesses.resize(config->numCpu, nullptr);
    coreQuantumCounters.resize(config->numCpu, 0);
    systemStartTime = std::chrono::high_resolution_clock::now();
    
    memoryManager = std::make_unique<MemoryManager>(
        config->maxOverallMemory, 
        config->memoryPerFrame,
        config->minMemoryPerProcess,
        config->maxMemoryPerProcess
    );
}

Scheduler::~Scheduler() {
    stop();
}

bool Scheduler::start() {
    if (isRunning.load()) {
        std::cout << "Scheduler is already running." << std::endl;
        return false;
    }
    
    shouldStop.store(false);
    isRunning.store(true);
    
    coreWorkers.clear();
    for (int i = 0; i < config->numCpu; ++i) {
        coreWorkers.emplace_back(&Scheduler::coreWorkerThread, this, i);
    }
    
    coreWorkers.emplace_back(&Scheduler::processCreatorThread, this);
    
    std::cout << "Scheduler started with " << config->numCpu << " CPU cores." << std::endl;
    return true;
}

bool Scheduler::startTestMode() {
    if (isRunning.load()) {
        std::cout << "Scheduler is already running." << std::endl;
        return false;
    }
    
    shouldStop.store(false);
    isRunning.store(true);
    
    coreWorkers.clear();
    for (int i = 0; i < config->numCpu; ++i) {
        coreWorkers.emplace_back(&Scheduler::coreWorkerThread, this, i);
    }
    
    coreWorkers.emplace_back(&Scheduler::testModeProcessCreator, this);
    
    std::cout << "Scheduler test mode started with " << config->numCpu << " CPU cores." << std::endl;
    return true;
}

void Scheduler::stop() {
    if (!isRunning.load()) {
        return;
    }
    
    shouldStop.store(true);
    isRunning.store(false);
    
    processCV.notify_all();
    
    for (auto& worker : coreWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    coreWorkers.clear();
    std::cout << "Scheduler stopped successfully." << std::endl;
}

void Scheduler::coreWorkerThread(int coreId) {
    while (!shouldStop.load()) {
        std::shared_ptr<Process> currentProcess = nullptr;
        
        {
            std::unique_lock<std::mutex> lock(processMutex);
            
            currentProcess = runningProcesses[coreId];
            
            if (!currentProcess && !readyQueue.empty()) {
                currentProcess = readyQueue.front();
                readyQueue.pop();
                
                runningProcesses[coreId] = currentProcess;
                currentProcess->state = ProcessState::RUNNING;
                currentProcess->coreAssignment = coreId;
            }
        }
        
        if (currentProcess) {
            if (currentProcess->sleepRemaining > 0) {
                currentProcess->sleepRemaining--;
                
                if (currentProcess->sleepRemaining == 0) {
                    currentProcess->state = ProcessState::READY;
                    
                    std::lock_guard<std::mutex> lock(processMutex);
                    currentProcess->coreAssignment = -1;
                    readyQueue.push(currentProcess);
                    runningProcesses[coreId] = nullptr;
                    processCV.notify_one();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(config->delayPerExec));
                }
                continue;
            }
            
            int instructionsPerChunk = 1; 
            int effectiveDelay = config->delayPerExec; 
            
            if (config->delayPerExec <= 5) {
                instructionsPerChunk = 8; 
            }
            
            int instructionsExecuted = 0;
            while (instructionsExecuted < instructionsPerChunk && !currentProcess->isComplete()) {
                std::string logEntry = currentProcess->executeNextInstruction();
                instructionsExecuted++;
                
                if (currentProcess->isComplete()) {
                    break;
                }
                
                if (currentProcess->state == ProcessState::WAITING && currentProcess->sleepRemaining > 0) {
                    break;
                }
            }
            
            if (currentProcess->isComplete()) {
                handleProcessCompletion(currentProcess);
                
                std::lock_guard<std::mutex> lock(processMutex);
                runningProcesses[coreId] = nullptr;
            }
            else if (currentProcess->state == ProcessState::WAITING && currentProcess->sleepRemaining > 0) {
            }
            else if (config->scheduler == "rr") {
                coreQuantumCounters[coreId] += instructionsExecuted;
                
                if (coreQuantumCounters[coreId] >= config->quantumCycles) {
                    coreQuantumCounters[coreId] = 0;
                    
                    std::lock_guard<std::mutex> lock(processMutex);
                    currentProcess->state = ProcessState::READY;
                    currentProcess->coreAssignment = -1;
                    readyQueue.push(currentProcess);
                    runningProcesses[coreId] = nullptr;
                    processCV.notify_one();
                }
            }
            
            if (effectiveDelay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(effectiveDelay));
            } else {
                std::this_thread::yield();
            }
        } else {
            std::unique_lock<std::mutex> lock(processMutex);
            processCV.wait_for(lock, std::chrono::milliseconds(50));
        }
    }
}

void Scheduler::processCreatorThread() {
    while (!shouldStop.load()) {
        if (dummyProcessGenerationEnabled.load()) {
            int activeCores = 0;
            int queueSize = 0;
            
            {
                std::lock_guard<std::mutex> lock(processMutex);
                for (const auto& process : runningProcesses) {
                    if (process != nullptr) activeCores++;
                }
                queueSize = readyQueue.size();
            }
            
            int processesToCreate = 1; 
            
            if (config->delayPerExec <= 5) {
                int totalWorkload = activeCores + queueSize;
                int desiredWorkload = config->numCpu + 5; 
                
                if (totalWorkload < desiredWorkload) {
                    processesToCreate = desiredWorkload - totalWorkload;
                }
                
                if (config->delayPerExec == 0 && totalWorkload < config->numCpu * 2) {
                    processesToCreate = std::max(processesToCreate, 2);
                }
            }
            
            for (int i = 0; i < processesToCreate; ++i) {
                createProcess();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(config->batchProcessFreq));
    }
}

void Scheduler::testModeProcessCreator() {
    while (!shouldStop.load()) {
        int activeCores = 0;
        int queueSize = 0;
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            for (const auto& process : runningProcesses) {
                if (process != nullptr) activeCores++;
            }
            queueSize = readyQueue.size();
        }
        
        int totalWorkload = activeCores + queueSize;
        int desiredWorkload = config->numCpu * 2;
        
        int processesToCreate = std::max(1, desiredWorkload - totalWorkload);
        
        for (int i = 0; i < processesToCreate; ++i) {
            createProcess();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Scheduler::handleProcessCompletion(std::shared_ptr<Process> process) {
    process->state = ProcessState::TERMINATED;
    process->updateMetrics();
    process->coreAssignment = -1;
    
    {
        std::lock_guard<std::mutex> lock(processMutex);
        terminatedProcesses.push_back(process);
    }
}

bool Scheduler::createProcess(const std::string& name) {
    ensureSchedulerStarted();
    
    std::string processName = name;
    if (processName.empty()) {
        processName = "process" + std::to_string(processCounter.fetch_add(1));
    }
    
    auto process = std::make_shared<Process>(processName);
    
    int baseInstructionCount = Utils::generateRandomInt(
        config->minInstructions, config->maxInstructions);
    
    int instructionCount = baseInstructionCount;
    
    process->generateInstructions(instructionCount);
    
    process->state = ProcessState::READY;
    
    {
        std::lock_guard<std::mutex> lock(processMutex);
        allProcesses.push_back(process);
        readyQueue.push(process);
    }
    
    processCV.notify_one();
    
    return true;
}

bool Scheduler::createProcess(const std::string& name, size_t memorySize) {
    ensureSchedulerStarted();
    
    if (!memoryManager->allocateMemory(name, memorySize)) {
        std::cout << "Invalid memory allocation" << std::endl;
        return false;
    }
    
    auto process = std::make_shared<Process>(name, memorySize);
    
    int baseInstructionCount = Utils::generateRandomInt(
        config->minInstructions, config->maxInstructions);
    
    process->generateInstructions(baseInstructionCount);
    process->state = ProcessState::READY;
    
    {
        std::lock_guard<std::mutex> lock(processMutex);
        allProcesses.push_back(process);
        readyQueue.push(process);
    }
    
    processCV.notify_one();
    return true;
}

bool Scheduler::createProcess(const std::string& name, size_t memorySize, const std::vector<std::string>& instructions) {
    ensureSchedulerStarted();
    
    if (instructions.size() < 1 || instructions.size() > 50) {
        std::cout << "Invalid command" << std::endl;
        return false;
    }
    
    if (!memoryManager->allocateMemory(name, memorySize)) {
        std::cout << "Invalid memory allocation" << std::endl;
        return false;
    }
    
    auto process = std::make_shared<Process>(name, memorySize, instructions);
    process->state = ProcessState::READY;
    
    {
        std::lock_guard<std::mutex> lock(processMutex);
        allProcesses.push_back(process);
        readyQueue.push(process);
    }
    
    processCV.notify_one();
    return true;
}

void Scheduler::displaySystemStatus() const {
    std::lock_guard<std::mutex> lock(processMutex);
    
    int busyCores = 0;
    for (const auto& process : runningProcesses) {
        if (process != nullptr) busyCores++;
    }
    
    int totalCores = config->numCpu;
    int coresAvailable = totalCores - busyCores;
    double cpuUtilization = (static_cast<double>(busyCores) / totalCores) * 100.0;
    
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << "CPU Status:" << std::endl;
    std::cout << "Total Cores      : " << totalCores << std::endl;
    std::cout << "Cores Used       : " << busyCores << std::endl;
    std::cout << "Cores Available  : " << coresAvailable << std::endl;
    std::cout << "CPU Utilization  : " << static_cast<int>(cpuUtilization) << "%" << std::endl << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << "Running processes:" << std::endl;
}

void Scheduler::displayProcesses() const {
    std::lock_guard<std::mutex> lock(processMutex);
    
    bool hasRunningProcesses = false;
    for (size_t i = 0; i < runningProcesses.size(); ++i) {
        if (runningProcesses[i]) {
            auto& p = runningProcesses[i];
            std::cout << std::left << std::setw(12) << p->name << "  ";
            std::cout << "(Started: " << p->creationTimestamp << ")  ";
            std::cout << "Core: " << i << "  ";
            std::cout << p->executedInstructions << " / " << p->totalInstructions << std::endl;
            hasRunningProcesses = true;
        }
    }
    
    if (!hasRunningProcesses) {
        std::cout << "No processes currently running." << std::endl;
    }
    
    std::cout << std::endl << "Finished processes:" << std::endl;
    if (terminatedProcesses.empty()) {
        std::cout << "No processes have finished yet." << std::endl;
    } else {
        for (const auto& p : terminatedProcesses) {
            std::cout << std::left << std::setw(12) << p->name << "  ";
            std::cout << "(" << p->completionTimestamp << ")  ";
            std::cout << "Finished  ";
            std::cout << p->executedInstructions << " / " << p->totalInstructions << std::endl;
        }
    }
    std::cout << "---------------------------------------------" << std::endl;
}

void Scheduler::generateReport(const std::string& filename) const {
    std::unique_lock<std::mutex> lock(processMutex, std::defer_lock);
    if (!lock.try_lock()) {
        std::cout << "System busy, please try generating report again." << std::endl;
        return;
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create report file: " << filename << std::endl;
        return;
    }
    
    file << "CSOPESY OS Emulator Report" << std::endl;
    file << "Generated: " << Utils::getCurrentTimestamp() << std::endl;
    file << std::endl;
    
    int busyCores = 0;
    for (const auto& process : runningProcesses) {
        if (process != nullptr) busyCores++;
    }
    
    int totalCores = config->numCpu;
    int coresAvailable = totalCores - busyCores;
    double cpuUtilization = (static_cast<double>(busyCores) / totalCores) * 100.0;
    
    file << "---------------------------------------------" << std::endl;
    file << "CPU Status:" << std::endl;
    file << "Total Cores      : " << totalCores << std::endl;
    file << "Cores Used       : " << busyCores << std::endl;
    file << "Cores Available  : " << coresAvailable << std::endl;
    file << "CPU Utilization  : " << static_cast<int>(cpuUtilization) << "%" << std::endl;
    
    file << "\n---------------------------------------------" << std::endl;
    file << "Running processes:" << std::endl;
    bool hasRunningProcesses = false;
    for (size_t i = 0; i < runningProcesses.size(); ++i) {
        if (runningProcesses[i]) {
            auto& p = runningProcesses[i];
            file << std::left << std::setw(12) << p->name << "  ";
            file << "(Started: " << p->creationTimestamp << ")  ";
            file << "Core: " << i << "  ";
            file << p->executedInstructions << " / " << p->totalInstructions << std::endl;
            hasRunningProcesses = true;
        }
    }
    
    if (!hasRunningProcesses) {
        file << "No processes currently running." << std::endl;
    }
    
    file << "\nFinished processes:" << std::endl;
    if (terminatedProcesses.empty()) {
        file << "No processes have finished yet." << std::endl;
    } else {
        for (const auto& p : terminatedProcesses) {
            file << std::left << std::setw(12) << p->name << "  ";
            file << "(" << p->completionTimestamp << ")  ";
            file << "Finished  ";
            file << p->executedInstructions << " / " << p->totalInstructions << std::endl;
        }
    }
    file << "---------------------------------------------" << std::endl;
    
    file.close();
    std::cout << "Report generated: " << filename << std::endl;
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) const {
    std::lock_guard<std::mutex> lock(processMutex);
    
    for (const auto& process : allProcesses) {
        if (process->name == name || process->pid == name) {
            return process;
        }
    }
    return nullptr;
}

bool Scheduler::isSystemRunning() const { return isRunning.load(); }

void Scheduler::ensureSchedulerStarted() {
    if (!isRunning.load()) {
        start();
    }
}

void Scheduler::enableDummyProcessGeneration() {
    dummyProcessGenerationEnabled.store(true);
}

void Scheduler::disableDummyProcessGeneration() {
    dummyProcessGenerationEnabled.store(false);
    std::cout << "Dummy process generation disabled." << std::endl;
}

bool Scheduler::isDummyGenerationEnabled() const { return dummyProcessGenerationEnabled.load(); }
