#include "scheduler.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <random>
#include <thread>
#include <chrono>
#include <algorithm>

Scheduler::Scheduler(const schedConfig& config) {
    numCores = config.numCores;
    algorithm = config.algorithm;
    quantumCycles = config.quantumCycles;
    batchProcFreq = config.batchProcFreq;
    minIns = config.minIns;
    maxIns = config.maxIns;
    delayPerExec = config.delayPerExec;
    coreToProcess.resize(config.numCores, "");
    memoryManager = std::make_unique<MemoryManager>(config.maxOverallMem);
    memPerProc = config.memPerProc;
    memPerFrame = config.memPerFrame;
    maxOverallMem = config.maxOverallMem;
}

bool Scheduler::addProcess(Process& process, bool suppressError) {
    process.memoryRequired = memPerProc;
    int addr = memoryManager->allocate(process.memoryRequired, process.name);
    if (addr == -1) {
        if (!suppressError) {
            std::cout << "Failed to allocate memory for process '" << process.name << "' (" << process.memoryRequired << " units).\n";
        }
        return false;
    }
    process.memoryAddress = addr;
    processList.push_back(process);
    return true;
}

const std::vector<Process>& Scheduler::getProcessList() const {
    return processList;
}

const std::vector<int>& Scheduler::getFinishedProcesses() const {
    return finishedProcesses;
}

void Scheduler::startScheduler() {
    if (schedulerRunning) {
        std::cout << "Scheduler already running.\n";
        return;
    }
    schedulerRunning = true;
    schedulerMain = std::thread(&Scheduler::runScheduler, this);
}

bool Scheduler::isRunning() const {
    return schedulerRunning;
}

void Scheduler::runScheduler() {
    int quantumCounter = 0;
    std::vector<std::thread> cpuThreads;
    for (int i = 1; i <= static_cast<int>(numCores); i++) {
        if (algorithm == "rr") {
            cpuThreads.emplace_back(&Scheduler::cpuWorkerRoundRobin, this, i);
        }
        else if (algorithm == "fcfs") {
            cpuThreads.emplace_back(&Scheduler::cpuWorker, this, i);
        }
        else {
            std::cout << "Algorithm Invalid\n";
            return;
        }
    }

    std::thread processCreator([this]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(minIns, maxIns);
        int processCounter = 1;

        while (schedulerRunning) {
            if (!schedulerRunning) break;
            Process proc;
            proc.name = "process" + std::to_string(processCounter++);
            proc.totalCommands = dist(gen);
            proc.executedCommands = 0;
            proc.startTimestamp = getCurrentTimestamp();
            proc.finished = false;
            proc.coreAssigned = -1;
            // You must implement generateRandomInstructions elsewhere
            // proc.instructions = generateRandomInstructions(proc.name, proc.totalCommands, 0);

            {
                std::lock_guard<std::mutex> lock(processMutex);
                pendingQueue.push_back(proc);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(batchProcFreq));
        }
    });

    std::thread pendingAllocator([this]() {
        while (schedulerRunning) {
            {
                std::lock_guard<std::mutex> lock(processMutex);
                if (!pendingQueue.empty()) {
                    size_t n = pendingQueue.size();
                    for (size_t i = 0; i < n; ++i) {
                        Process proc = pendingQueue.front();
                        pendingQueue.pop_front();
                        bool added = addProcess(proc, true);
                        if (!added) {
                            pendingQueue.push_back(proc);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::thread schedThread(&Scheduler::scheduler, this);

    if (processCreator.joinable()) processCreator.join();
    if (pendingAllocator.joinable()) pendingAllocator.join();
    if (schedThread.joinable()) schedThread.join();
    for (auto& t : cpuThreads) t.join();
}

void Scheduler::generateLog(const std::string& filename) {
    std::ofstream reportFile(filename);
    if (!reportFile.is_open()) {
        std::cerr << "Failed to open " << filename << " for writing.\n";
        return;
    }

    reportFile << std::left;
    int nameWidth = 12;

    int totalCores = static_cast<int>(coreToProcess.size());
    int coresUsed = 0;
    for (const auto& procName : coreToProcess) {
        if (!procName.empty()) {
            coresUsed++;
        }
    }
    int coresAvailable = totalCores - coresUsed;
    double cpuUtil = (static_cast<double>(coresUsed) / totalCores) * 100.0;
    reportFile << "---------------------------------------------\n";
    reportFile << "CPU Status:\n";
    reportFile << "Total Cores      : " << totalCores << "\n";
    reportFile << "Cores Used       : " << coresUsed << "\n";
    reportFile << "Cores Available  : " << coresAvailable << "\n";
    reportFile << "CPU Utilization  : " << static_cast<int>(cpuUtil) << "%\n\n";
    reportFile << "---------------------------------------------\n";
    reportFile << "Running processes:\n";
    for (int core = 0; core < coreToProcess.size(); ++core) {
        const std::string& pname = coreToProcess[core];
        if (pname.empty()) continue;

        for (const auto& proc : processList) {
            if (proc.name == pname) {
                reportFile << std::setw(nameWidth) << proc.name << "  ";
                reportFile << "(Started: " << proc.startTimestamp << ")  ";
                reportFile << "Core: " << core << "  ";
                reportFile << proc.executedCommands << " / " << proc.totalCommands << "\n";
                break;
            }
        }
    }
    reportFile << "\nFinished processes:\n";
    for (size_t i = 0; i < processList.size(); i++) {
        if (processList[i].finished) {
            reportFile << std::setw(nameWidth) << processList[i].name << "  ";
            reportFile << "(" << processList[i].finishTimestamp << ")  ";
            reportFile << "Finished  ";
            reportFile << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
        }
    }
    reportFile << "---------------------------------------------\n";
}

void Scheduler::shutdown() {
    schedulerRunning = false;
    if (schedulerMain.joinable()) {
        schedulerMain.join();
    }
    for (const auto& proc : processList) {
        if (proc.finished && proc.memoryAddress != -1) {
            memoryManager->free(proc.name);
        }
    }
}

void Scheduler::printMemory() const {
    memoryManager->printMemory();
}

void Scheduler::printConfig() const {
    std::cout << "---- Scheduler Configuration ----\n";
    std::cout << "Number of CPU Cores   : " << numCores << "\n";
    std::cout << "Scheduling Algorithm  : " << algorithm << "\n";
    std::cout << "Quantum Cycles        : " << quantumCycles << "\n";
    std::cout << "Batch Process Freq    : " << batchProcFreq << "\n";
    std::cout << "Min Instructions      : " << minIns << "\n";
    std::cout << "Max Instructions      : " << maxIns << "\n";
    std::cout << "Delay per Execution   : " << delayPerExec << "\n";
    std::cout << "Max Overall Memory    : " << maxOverallMem << "\n";
    std::cout << "Memory Per Frame      : " << memPerFrame << "\n";
    std::cout << "Memory Per Process    : " << memPerProc << "\n";
    std::cout << "----------------------------------\n";
}

void Scheduler::showScreenLS() {
    processMutex.lock();
    std::cout << std::left;
    int nameWidth = 12;

    int totalCores = static_cast<int>(coreToProcess.size());
    int coresUsed = 0;
    for (const auto& procName : coreToProcess) {
        if (!procName.empty()) {
            coresUsed++;
        }
    }
    int coresAvailable = totalCores - coresUsed;
    double cpuUtil = (static_cast<double>(coresUsed) / totalCores) * 100.0;
    std::cout << "---------------------------------------------\n";
    std::cout << "CPU Status:\n";
    std::cout << "Total Cores      : " << totalCores << "\n";
    std::cout << "Cores Used       : " << coresUsed << "\n";
    std::cout << "Cores Available  : " << coresAvailable << "\n";
    std::cout << "CPU Utilization  : " << static_cast<int>(cpuUtil) << "%\n\n";
    std::cout << "---------------------------------------------\n";
    std::cout << "Running processes:\n";
    for (int core = 0; core < coreToProcess.size(); ++core) {
        const std::string& pname = coreToProcess[core];
        if (pname.empty()) continue;

        for (const auto& proc : processList) {
            if (proc.name == pname) {
                std::cout << std::setw(nameWidth) << proc.name << "  ";
                std::cout << "(Started: " << proc.startTimestamp << ")  ";
                std::cout << "Core: " << core << "  ";
                std::cout << proc.executedCommands << " / " << proc.totalCommands << "\n";
                break;
            }
        }
    }
    std::cout << "\nFinished processes:\n";
    for (size_t i = 0; i < processList.size(); i++) {
        if (processList[i].finished) {
            std::cout << std::setw(nameWidth) << processList[i].name << "  ";
            std::cout << "(" << processList[i].finishTimestamp << ")  ";
            std::cout << "Finished  ";
            std::cout << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
        }
    }
    std::cout << "---------------------------------------------\n";
    processMutex.unlock();
}

void Scheduler::setScreenMap(std::map<std::string, ScreenSession>* screens) {
    screenSessions = screens;
}

void Scheduler::freeProcessMemory(const std::string& name) {
    memoryManager->free(name);
}

std::mutex& Scheduler::getProcessMutex() {
    return processMutex;
}

std::vector<std::string>& Scheduler::getCoreToProcess() {
    return coreToProcess;
}

// Private methods (implement as needed)
void Scheduler::cpuWorker(int coreID) {
    // Implementation omitted for brevity; see OS_Emu_Project.cpp for logic.
}

void Scheduler::cpuWorkerRoundRobin(int coreID) {
    // Implementation omitted for brevity; see OS_Emu_Project.cpp for logic.
}

void Scheduler::scheduler() {
    // Implementation omitted for brevity; see OS_Emu_Project.cpp for logic.
}

void Scheduler::writeMemoryReport(int quantumCycle) {
    std::ostringstream fname;
    fname << "memory_stamp_" << quantumCycle << ".txt";
    std::ofstream out(fname.str());
    if (!out.is_open()) return;
    out << "Timestamp: (" << getCurrentTimestamp() << ")\n";
    out << "Number of processes in memory: " << memoryManager->countProcessesInMemory() << "\n";
    out << "Total external fragmentation in KB: " << memoryManager->getExternalFragmentation() << "\n";
    out << memoryManager->asciiMemoryMap();
    out.close();
}