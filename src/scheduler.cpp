#include "../headers/scheduler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>

Scheduler::Scheduler(const schedConfig& config) 
    : numCores(config.numCores), algorithm(config.algorithm), quantumCycles(config.quantumCycles),
      batchProcFreq(config.batchProcFreq), minIns(config.minIns), maxIns(config.maxIns),
      delayPerExec(config.delayPerExec), maxOverallMem(config.maxOverallMem),
      memPerFrame(config.memPerFrame), memPerProc(config.memPerProc) {
    
    coreToProcess.resize(numCores, "");
    memoryManager = std::make_unique<MemoryManager>(maxOverallMem);
}

Scheduler::~Scheduler() {
    shutdown();
}

bool Scheduler::addProcess(const Process& proc, bool suppressError) {
    int memAddr = memoryManager->allocate(memPerProc, proc.name);
    if (memAddr == -1) {
        if (!suppressError) {
            std::cout << "Failed to allocate memory for process: " << proc.name << std::endl;
        }
        return false;
    }

    Process newProc = proc;
    newProc.memoryAddress = memAddr;
    newProc.quantumRemaining = quantumCycles;

    std::lock_guard<std::mutex> lock(processMutex);
    processList.push_back(newProc);
    
    if (schedulerRunning) {
        readyQueue.push_back(newProc);
    }
    
    return true;
}

void Scheduler::freeProcessMemory(const std::string& processName) {
    memoryManager->free(processName);
}

void Scheduler::startScheduler() {
    if (schedulerRunning) {
        std::cout << "Scheduler is already running.\n";
        return;
    }
    
    std::cout << "Scheduler started.\n";
    schedulerRunning = true;
    schedulerMain = std::thread(&Scheduler::runScheduler, this);
}

void Scheduler::scheduler() {
    while (schedulerRunning) {
        processSchedulingAlgorithm();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Scheduler::processSchedulingAlgorithm() {
    std::lock_guard<std::mutex> lock(processMutex);
    
    if (algorithm == "fcfs") {
        for (int core = 0; core < numCores; ++core) {
            if (coreToProcess[core].empty() && !readyQueue.empty()) {
                Process proc = readyQueue.front();
                readyQueue.pop_front();
                
                int procIndex = findProcessIndex(proc.name);
                if (procIndex != -1 && !processList[procIndex].finished) {
                    processList[procIndex].coreAssigned = core;
                    coreToProcess[core] = proc.name;
                }
            }
        }
    } else if (algorithm == "rr") {
        for (int core = 0; core < numCores; ++core) {
            if (coreToProcess[core].empty() && !readyQueue.empty()) {
                Process proc = readyQueue.front();
                readyQueue.pop_front();
                
                int procIndex = findProcessIndex(proc.name);
                if (procIndex != -1 && !processList[procIndex].finished) {
                    processList[procIndex].coreAssigned = core;
                    processList[procIndex].quantumRemaining = quantumCycles;
                    coreToProcess[core] = proc.name;
                }
            }
        }
    }
}

int Scheduler::findProcessIndex(const std::string& name) const {
    for (size_t i = 0; i < processList.size(); ++i) {
        if (processList[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Scheduler::cpuWorker(int coreId) {
    while (schedulerRunning) {
        std::string processName;
        
        {
            std::lock_guard<std::mutex> lock(processMutex);
            processName = coreToProcess[coreId];
        }
        
        if (!processName.empty()) {
            std::lock_guard<std::mutex> lock(processMutex);
            int procIndex = findProcessIndex(processName);
            
            if (procIndex != -1 && !processList[procIndex].finished) {
                Process& proc = processList[procIndex];
                
                if (proc.sleepRemaining > 0) {
                    proc.sleepRemaining--;
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
                    continue;
                }
                
                if (proc.executedCommands < proc.instructions.size()) {
                    std::ostringstream ss;
                    ss << "(" << getCurrentTimestamp() << ") Core:" << coreId << " ";
                    
                    Instruction& instr = proc.instructions[proc.executedCommands];
                    executeInstruction(proc, instr, ss);
                    
                    if (screenSessions) {
                        auto it = screenSessions->find(proc.name);
                        if (it != screenSessions->end()) {
                            it->second.processLogs.push_back(ss.str());
                            it->second.currentLine = proc.executedCommands;
                        }
                    }
                }
                
                proc.executedCommands++;
                
                if (algorithm == "rr") {
                    proc.quantumRemaining--;
                    if (proc.quantumRemaining <= 0 && proc.executedCommands < proc.totalCommands) {
                        readyQueue.push_back(proc);
                        proc.coreAssigned = -1;
                        coreToProcess[coreId] = "";
                        continue;
                    }
                }
                
                if (proc.executedCommands >= proc.totalCommands) {
                    proc.finished = true;
                    proc.finishTimestamp = getCurrentTimestamp();
                    proc.coreAssigned = -1;
                    coreToProcess[coreId] = "";
                    
                    if (proc.memoryAddress != -1) {
                        memoryManager->free(proc.name);
                    }
                }
            } else {
                coreToProcess[coreId] = "";
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
    }
}

void Scheduler::runScheduler() {
    cpuThreads.clear();
    for (int i = 0; i < numCores; ++i) {
        cpuThreads.emplace_back(&Scheduler::cpuWorker, this, i);
    }

    std::thread processCreator([this]() {
        int processCounter = 1;
        while (schedulerRunning) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(minIns, maxIns);
            int randomInstructions = dist(gen);

            Process proc;
            proc.name = "p" + std::to_string(processCounter++);
            proc.totalCommands = randomInstructions;
            proc.executedCommands = 0;
            proc.finished = false;
            proc.coreAssigned = -1;
            proc.startTimestamp = getCurrentTimestamp();
            proc.instructions = generateRandomInstructions(proc.name, proc.totalCommands, 0);

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
                        if (added) {
                            if (screenSessions) {
                                (*screenSessions)[proc.name] = {
                                    proc.name, 1, proc.totalCommands, proc.startTimestamp
                                };
                            }
                        } else {
                            pendingQueue.push_back(proc);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::thread schedThread(&Scheduler::scheduler, this);

    // Detach threads so they run in background and don't block main thread
    processCreator.detach();
    pendingAllocator.detach();
    schedThread.detach();
    for (auto& t : cpuThreads) {
        t.detach();
    }
    
    // Clear the vector since we detached the threads
    cpuThreads.clear();
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
    
    // Give detached threads time to see the flag and exit gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
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
    std::lock_guard<std::mutex> lock(processMutex);
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
}

void Scheduler::setScreenMap(std::map<std::string, ScreenSession>* screens) {
    screenSessions = screens;
}
