#include "emulator.h"

Scheduler::Scheduler(std::unique_ptr<SystemConfig> cfg) 
    : config(std::move(cfg)) {
    
    runningProcesses.resize(config->numCpu, nullptr);
    systemStartTime = std::chrono::high_resolution_clock::now();
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
            std::string logEntry = currentProcess->executeNextInstruction();
            
            if (!logEntry.empty()) {
                std::cout << "Core " << coreId << ": " << currentProcess->name 
                          << " - " << logEntry << std::endl;
            }
            
            if (currentProcess->isComplete()) {
                handleProcessCompletion(currentProcess);
                
                std::lock_guard<std::mutex> lock(processMutex);
                runningProcesses[coreId] = nullptr;
            }
            else if (config->scheduler == "rr") {
                static std::map<int, int> coreQuantumCounters;
                coreQuantumCounters[coreId]++;
                
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
            
            std::this_thread::sleep_for(std::chrono::milliseconds(config->delayPerExec));
        } else {
            std::unique_lock<std::mutex> lock(processMutex);
            processCV.wait_for(lock, std::chrono::milliseconds(50));
        }
    }
}

void Scheduler::processCreatorThread() {
    while (!shouldStop.load()) {
        if (createProcess()) {
            std::cout << "New process created automatically." << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config->batchProcessFreq * 1000));
    }
}

bool Scheduler::createProcess(const std::string& name) {
    std::string processName = name;
    if (processName.empty()) {
        processName = "process" + std::to_string(processCounter.fetch_add(1));
    }
    
    auto process = std::make_shared<Process>(processName);
    
    int instructionCount = Utils::generateRandomInt(
        config->minInstructions, config->maxInstructions);
    process->generateInstructions(instructionCount);
    
    process->state = ProcessState::READY;
    
    {
        std::lock_guard<std::mutex> lock(processMutex);
        allProcesses.push_back(process);
        readyQueue.push(process);
    }
    
    processCV.notify_one();
    
    std::cout << "Process '" << processName << "' created with " 
              << instructionCount << " instructions." << std::endl;
    return true;
}

void Scheduler::handleProcessCompletion(std::shared_ptr<Process> process) {
    process->state = ProcessState::TERMINATED;
    process->updateMetrics();
    process->coreAssignment = -1;
    
    {
        std::lock_guard<std::mutex> lock(processMutex);
        terminatedProcesses.push_back(process);
    }
    
    std::cout << "Process '" << process->name << "' completed. "
              << "Turnaround time: " << process->turnaroundTime << "ms, "
              << "Waiting time: " << process->waitingTime << "ms" << std::endl;
}

void Scheduler::displayProcesses() const {
    std::lock_guard<std::mutex> lock(processMutex);
    
    std::cout << "\n=== Process Status ===" << std::endl;
    std::cout << std::left << std::setw(12) << "PID" 
              << std::setw(15) << "Name" 
              << std::setw(10) << "State"
              << std::setw(8) << "Core"
              << std::setw(12) << "Progress" << std::endl;
    std::cout << std::string(57, '-') << std::endl;
    
    for (size_t i = 0; i < runningProcesses.size(); ++i) {
        if (runningProcesses[i]) {
            auto& p = runningProcesses[i];
            std::cout << std::setw(12) << p->pid
                      << std::setw(15) << p->name
                      << std::setw(10) << p->getStateString()
                      << std::setw(8) << i
                      << std::setw(12) << (p->executedInstructions + "/" + std::to_string(p->totalInstructions)) << std::endl;
        }
    }
    
    auto readyQueueCopy = readyQueue;
    while (!readyQueueCopy.empty()) {
        auto& p = readyQueueCopy.front();
        readyQueueCopy.pop();
        std::cout << std::setw(12) << p->pid
                  << std::setw(15) << p->name
                  << std::setw(10) << p->getStateString()
                  << std::setw(8) << "-"
                  << std::setw(12) << (p->executedInstructions + "/" + std::to_string(p->totalInstructions)) << std::endl;
    }
    
    int displayCount = std::min(5, static_cast<int>(terminatedProcesses.size()));
    for (int i = terminatedProcesses.size() - displayCount; i < terminatedProcesses.size(); ++i) {
        auto& p = terminatedProcesses[i];
        std::cout << std::setw(12) << p->pid
                  << std::setw(15) << p->name
                  << std::setw(10) << p->getStateString()
                  << std::setw(8) << "-"
                  << std::setw(12) << "FINISHED" << std::endl;
    }
    
    std::cout << std::string(57, '-') << std::endl;
}

void Scheduler::displaySystemStatus() const {
    std::lock_guard<std::mutex> lock(processMutex);
    
    int busyCores = 0;
    for (const auto& process : runningProcesses) {
        if (process != nullptr) busyCores++;
    }
    
    double cpuUtilization = (static_cast<double>(busyCores) / config->numCpu) * 100.0;
    
    std::cout << "\n=== System Status ===" << std::endl;
    std::cout << "CPU Cores: " << config->numCpu << std::endl;
    std::cout << "Cores Used: " << busyCores << std::endl;
    std::cout << "Cores Available: " << (config->numCpu - busyCores) << std::endl;
    std::cout << "CPU Utilization: " << std::fixed << std::setprecision(1) 
              << cpuUtilization << "%" << std::endl;
    
    std::cout << "\nProcess Counts:" << std::endl;
    std::cout << "  Total: " << allProcesses.size() << std::endl;
    std::cout << "  Running: " << busyCores << std::endl;
    std::cout << "  Ready: " << readyQueue.size() << std::endl;
    std::cout << "  Terminated: " << terminatedProcesses.size() << std::endl;
    
    if (!terminatedProcesses.empty()) {
        double avgTurnaround = 0, avgWaiting = 0;
        for (const auto& p : terminatedProcesses) {
            avgTurnaround += p->turnaroundTime;
            avgWaiting += p->waitingTime;
        }
        avgTurnaround /= terminatedProcesses.size();
        avgWaiting /= terminatedProcesses.size();
        
        std::cout << "\nPerformance Metrics:" << std::endl;
        std::cout << "  Avg Turnaround Time: " << std::fixed << std::setprecision(2) 
                  << avgTurnaround << "ms" << std::endl;
        std::cout << "  Avg Waiting Time: " << std::fixed << std::setprecision(2) 
                  << avgWaiting << "ms" << std::endl;
    }
    
    auto uptime = std::chrono::high_resolution_clock::now() - systemStartTime;
    std::cout << "System Uptime: " << Utils::formatDuration(
        std::chrono::duration_cast<std::chrono::milliseconds>(uptime)) << std::endl;
    
    std::cout << "=====================" << std::endl;
}

void Scheduler::generateReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create report file: " << filename << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(processMutex);
    
    file << "CSOPESY Process Scheduler Report" << std::endl;
    file << "Generated: " << Utils::getCurrentTimestamp() << std::endl;
    file << std::string(50, '=') << std::endl;
    
    file << "\nSystem Configuration:" << std::endl;
    file << "CPU Cores: " << config->numCpu << std::endl;
    file << "Scheduler Algorithm: " << config->scheduler << std::endl;
    file << "Quantum Cycles: " << config->quantumCycles << std::endl;
    
    int busyCores = 0;
    for (const auto& process : runningProcesses) {
        if (process != nullptr) busyCores++;
    }
    
    file << "\nCurrent System Status:" << std::endl;
    file << "CPU Utilization: " << std::fixed << std::setprecision(1) 
         << (static_cast<double>(busyCores) / config->numCpu) * 100.0 << "%" << std::endl;
    file << "Active Processes: " << busyCores << std::endl;
    file << "Ready Queue: " << readyQueue.size() << std::endl;
    file << "Completed Processes: " << terminatedProcesses.size() << std::endl;
    
    file << "\nProcess Details:" << std::endl;
    file << std::left << std::setw(12) << "PID" 
         << std::setw(15) << "Name"
         << std::setw(10) << "State"
         << std::setw(12) << "Instructions"
         << std::setw(12) << "Turnaround"
         << std::setw(12) << "Waiting" << std::endl;
    file << std::string(73, '-') << std::endl;
    
    for (const auto& process : allProcesses) {
        file << std::setw(12) << process->pid
             << std::setw(15) << process->name
             << std::setw(10) << process->getStateString()
             << std::setw(12) << (std::to_string(process->executedInstructions) + "/" + std::to_string(process->totalInstructions))
             << std::setw(12) << process->turnaroundTime
             << std::setw(12) << process->waitingTime << std::endl;
    }
    
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
