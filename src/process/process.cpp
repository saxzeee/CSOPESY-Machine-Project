#include "process.h"
#include "../utils/utils.h"
#include "../memory/memory_manager.h"
#include <chrono>
#include <random>
#include <sstream>
#include <algorithm>
#include <set>
#include <climits>
#include <iostream>
#include <iomanip>

Process::Process(const std::string& processName) 
    : name(processName), state(ProcessState::NEW), priority(0), 
      coreAssignment(-1),
      executedInstructions(0), totalInstructions(0) {
    
    static std::atomic<int> pidCounter{1};
    pid = "p" + std::string(3 - std::to_string(pidCounter.load()).length(), '0') + 
          std::to_string(pidCounter.fetch_add(1));
    
    creationTimestamp = Utils::getCurrentTimestamp();
    arrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

Process::Process(const std::string& processName, size_t memorySize) 
    : name(processName), state(ProcessState::NEW), priority(0), 
      coreAssignment(-1),
      executedInstructions(0), totalInstructions(0), allocatedMemory(memorySize) {
    
    static std::atomic<int> pidCounter{1};
    pid = "p" + std::string(3 - std::to_string(pidCounter.load()).length(), '0') + 
          std::to_string(pidCounter.fetch_add(1));
    
    creationTimestamp = Utils::getCurrentTimestamp();
    arrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

Process::Process(const std::string& processName, size_t memorySize, const std::vector<std::string>& customInstructions) 
    : name(processName), state(ProcessState::NEW), priority(0), 
      coreAssignment(-1),
      executedInstructions(0), totalInstructions(0), allocatedMemory(memorySize) {
    
    static std::atomic<int> pidCounter{1};
    pid = "p" + std::string(3 - std::to_string(pidCounter.load()).length(), '0') + 
          std::to_string(pidCounter.fetch_add(1));
    
    creationTimestamp = Utils::getCurrentTimestamp();
    arrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    setCustomInstructions(customInstructions);
}

void Process::generateInstructions(int count) {
    totalInstructions = count;
    remainingTime = count;
    burstTime = count;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::vector<std::string> varNames = {"x", "y", "z", "counter", "sum", "temp", "result", "value"};
    
    std::uniform_int_distribution<> varDist(0, varNames.size() - 1);
    std::uniform_int_distribution<> valueDist(1, 100);
    std::uniform_int_distribution<> sleepDist(1, 5);
    std::uniform_int_distribution<> forRepeatsDist(2, 5);
    
    std::vector<std::pair<std::string, int>> instructionWeights = {
        {"DECLARE", 15},   
        {"ADD", 20},       
        {"SUBTRACT", 15},    
        {"PRINT", 20},     
        {"SLEEP", 10},      
        {"FOR", 10},
        {"READ", 15},     
        {"WRITE", 15}      
    };
    
    std::vector<std::string> weightedInstructions;
    for (const auto& pair : instructionWeights) {
        for (int i = 0; i < pair.second; ++i) {
            weightedInstructions.push_back(pair.first);
        }
    }
    std::uniform_int_distribution<> instDist(0, weightedInstructions.size() - 1);
    
    std::set<std::string> declaredVars;
    int currentNestingLevel = 0; 
    
    for (int i = 0; i < count; ++i) {
        std::string instructionType = weightedInstructions[instDist(gen)];
        std::string instruction;
        
        if (instructionType == "DECLARE") {
            std::string var = varNames[varDist(gen)];
            int value = valueDist(gen);
            instruction = "DECLARE(" + var + ", " + std::to_string(value) + ")";
            declaredVars.insert(var);
            
        } else if (instructionType == "ADD") {
            std::string var1 = varNames[varDist(gen)];
            std::string var2 = varNames[varDist(gen)];
            
            if (gen() % 2 == 0) {
                int value = valueDist(gen);
                instruction = "ADD(" + var1 + ", " + var2 + ", " + std::to_string(value) + ")";
            } else {
                std::string var3 = varNames[varDist(gen)];
                instruction = "ADD(" + var1 + ", " + var2 + ", " + var3 + ")";
            }
            declaredVars.insert(var1);
            
        } else if (instructionType == "SUBTRACT") {
            std::string var1 = varNames[varDist(gen)];
            std::string var2 = varNames[varDist(gen)];
            
            if (gen() % 2 == 0) {
                int value = valueDist(gen);
                instruction = "SUBTRACT(" + var1 + ", " + var2 + ", " + std::to_string(value) + ")";
            } else {
                std::string var3 = varNames[varDist(gen)];
                instruction = "SUBTRACT(" + var1 + ", " + var2 + ", " + var3 + ")";
            }
            declaredVars.insert(var1);
            
        } else if (instructionType == "PRINT") {
            if (!declaredVars.empty() && gen() % 3 == 0) {
                auto it = declaredVars.begin();
                std::advance(it, gen() % declaredVars.size());
                std::string var = *it;
                instruction = "PRINT(\"Hello world from " + name + "!\" + " + var + ")";
            } else {
                instruction = "PRINT(\"Hello world from " + name + "!\")";
            }
            
        } else if (instructionType == "SLEEP") {
            int ticks = sleepDist(gen);
            instruction = "SLEEP(" + std::to_string(ticks) + ")";
            
        } else if (instructionType == "READ") {
            std::uniform_int_distribution<> addrDist(0, std::max(64, static_cast<int>(allocatedMemory - 2)));
            uint32_t address = addrDist(gen);
            address = (address / 2) * 2;
            
            std::ostringstream oss;
            oss << "READ 0x" << std::hex << std::uppercase << address;
            instruction = oss.str();
            
        } else if (instructionType == "WRITE") {
            std::uniform_int_distribution<> addrDist(0, std::max(64, static_cast<int>(allocatedMemory - 2)));
            uint32_t address = addrDist(gen);
            address = (address / 2) * 2;
            
            uint16_t value = static_cast<uint16_t>(valueDist(gen));
            
            std::ostringstream oss;
            oss << "WRITE 0x" << std::hex << std::uppercase << address << " " << std::dec << value;
            instruction = oss.str();
            
        } else if (instructionType == "FOR") {
            if (currentNestingLevel < 3) {
                currentNestingLevel++;
                
                int repeats = forRepeatsDist(gen);
                int innerInstructions = std::min(2, count - i - 1);
                
                std::vector<std::string> forBody;
                for (int j = 0; j < innerInstructions; ++j) {
                    std::string innerType = weightedInstructions[instDist(gen)];
                    
                    if (innerType == "FOR" && currentNestingLevel >= 3) {
                        innerType = "ADD"; 
                    }
                    
                    std::string innerInstruction;
                    if (innerType == "ADD") {
                        std::string var = "counter";
                        innerInstruction = "ADD(" + var + ", " + var + ", 1)";
                    } else if (innerType == "PRINT") {
                        innerInstruction = "PRINT(\"Hello world from " + name + "!\")";
                    } else if (innerType == "DECLARE") {
                        std::string var = varNames[varDist(gen)];
                        int value = valueDist(gen);
                        innerInstruction = "DECLARE(" + var + ", " + std::to_string(value) + ")";
                    } else if (innerType == "FOR" && currentNestingLevel < 3) {
                        innerInstruction = "FOR([ADD(counter, counter, 1)], 2)";
                        currentNestingLevel++; 
                    } else {
                        innerInstruction = "ADD(counter, counter, 1)";
                    }
                    forBody.push_back(innerInstruction);
                }
                
                instruction = "FOR([";
                for (size_t k = 0; k < forBody.size(); ++k) {
                    instruction += forBody[k];
                    if (k < forBody.size() - 1) instruction += ", ";
                }
                instruction += "], " + std::to_string(repeats) + ")";
                
                currentNestingLevel--;
            } else {
                std::string var1 = varNames[varDist(gen)];
                std::string var2 = varNames[varDist(gen)];
                int value = valueDist(gen);
                instruction = "ADD(" + var1 + ", " + var2 + ", " + std::to_string(value) + ")";
                declaredVars.insert(var1);
            }
        }
        
        pendingInstructions.push(instruction);
    }
}

std::string Process::executeNextInstruction() {
    if (pendingInstructions.empty()) {
        return "";
    }
    
    std::string instruction = pendingInstructions.front();
    pendingInstructions.pop();
    
    std::string timestamp = Utils::getCurrentTimestamp();
    std::string result = processInstruction(instruction);
    std::string logEntry = "(" + timestamp + ") Core:" + std::to_string(coreAssignment) + " " + instruction;
    
    if (!result.empty()) {
        logEntry += " -> " + result;
    }
    
    instructionHistory.push_back(logEntry);
    executedInstructions++;
    remainingTime--;
    
    if (responseTime == -1) {
        responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count() - arrivalTime;
    }
    
    return logEntry;
}

std::string Process::executeNextInstruction(MemoryManager* memoryManager) {
    if (pendingInstructions.empty()) {
        return "";
    }
    
    std::string instruction = pendingInstructions.front();
    pendingInstructions.pop();
    
    std::string timestamp = Utils::getCurrentTimestamp();
    
    if (memoryManager && allocatedMemory > 0) {
        uint32_t simulatedAddress = (executedInstructions * 4) % allocatedMemory;
        memoryManager->accessMemory(pid, simulatedAddress);
        
        if (executedInstructions % 3 == 0) {
            uint32_t writeAddress = (executedInstructions * 8) % allocatedMemory;
            memoryManager->writeMemory(pid, writeAddress, static_cast<uint16_t>(executedInstructions & 0xFFFF));
        }
        
        if (executedInstructions % 5 == 0) {
            uint32_t readAddress = (executedInstructions * 12) % allocatedMemory;
            memoryManager->readMemory(pid, readAddress);
        }
    }
    
    std::string result = processInstruction(instruction);
    std::string logEntry = "(" + timestamp + ") Core:" + std::to_string(coreAssignment) + " " + instruction;
    
    if (!result.empty()) {
        logEntry += " -> " + result;
    }
    
    instructionHistory.push_back(logEntry);
    executedInstructions++;
    remainingTime--;
    
    if (responseTime == -1) {
        responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count() - arrivalTime;
    }
    
    return logEntry;
}

std::string Process::processInstruction(const std::string& instruction) {
    std::string upperInst = instruction;
    std::transform(upperInst.begin(), upperInst.end(), upperInst.begin(), ::toupper);
    
    if (upperInst.find("DECLARE(") == 0) {
        return processDeclare(instruction);
    } else if (upperInst.find("ADD(") == 0) {
        return processAdd(instruction);
    } else if (upperInst.find("SUBTRACT(") == 0) {
        return processSubtract(instruction);
    } else if (upperInst.find("PRINT(") == 0) {
        return processPrint(instruction);
    } else if (upperInst.find("SLEEP(") == 0) {
        return processSleep(instruction);
    } else if (upperInst.find("FOR(") == 0) {
        return processFor(instruction);
    } else if (upperInst.find("READ ") == 0 || upperInst.find("READ\t") == 0) {
        return processRead(instruction);
    } else if (upperInst.find("WRITE ") == 0 || upperInst.find("WRITE\t") == 0) {
        return processWrite(instruction);
    }
    // space instead of () in commands
    else if (upperInst.find("DECLARE ") == 0) {
        // Convert to parenthesis style for handler
        std::string params = instruction.substr(8);
        return processDeclare("DECLARE(" + params + ")");
    } else if (upperInst.find("ADD ") == 0) {
        std::string params = instruction.substr(4);
        return processAdd("ADD(" + params + ")");
    } else if (upperInst.find("SUBTRACT ") == 0) {
        std::string params = instruction.substr(9);
        return processSubtract("SUBTRACT(" + params + ")");
    } else if (upperInst.find("PRINT ") == 0) {
        std::string params = instruction.substr(6);
        return processPrint("PRINT(" + params + ")");
    } else if (upperInst.find("SLEEP ") == 0) {
        std::string params = instruction.substr(6);
        return processSleep("SLEEP(" + params + ")");
    } else if (upperInst.find("FOR ") == 0) {
        std::string params = instruction.substr(4);
        return processFor("FOR(" + params + ")");
    } else if (upperInst.find("READ ") == 0 || upperInst.find("READ\t") == 0) {
        return processRead(instruction);
    }
    else if (upperInst.find("READ ") == 0 || upperInst.find("READ\t") == 0) {
        return processRead(instruction);
    } else if (upperInst.find("WRITE ") == 0 || upperInst.find("WRITE\t") == 0) {
        return processWrite(instruction);
    }
    else if (upperInst.find("PRINT ") == 0) {
        std::string params = instruction.substr(6);
        return processPrint("PRINT(" + params + ")");
    }

    return "Unknown instruction: " + instruction;
    return "Unknown instruction: " + instruction;
}

std::string Process::processDeclare(const std::string& instruction) {
    size_t start = instruction.find('(') + 1;
    size_t end = instruction.find(')', start);
    std::string params = instruction.substr(start, end - start);
    
    size_t comma = params.find(',');
    if (comma != std::string::npos) {
        std::string varName = params.substr(0, comma);
        std::string valueStr = params.substr(comma + 1);
        
        varName.erase(std::remove_if(varName.begin(), varName.end(), ::isspace), varName.end());
        valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
        
        int value = std::stoi(valueStr);
        uint16_t clampedValue = static_cast<uint16_t>(std::max(0, std::min(value, static_cast<int>(UINT16_MAX))));
        variables[varName] = clampedValue;
        
        return "Declared " + varName + " = " + std::to_string(clampedValue);
    }
    return "";
}

std::string Process::processAdd(const std::string& instruction) {
    size_t start = instruction.find('(') + 1;
    size_t end = instruction.find(')', start);
    std::string params = instruction.substr(start, end - start);
    
    std::vector<std::string> tokens;
    std::stringstream ss(params);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        tokens.push_back(token);
    }
    
    if (tokens.size() == 3) {
        std::string var1 = tokens[0];
        uint16_t val2 = getVariableOrValue(tokens[1]);
        uint16_t val3 = getVariableOrValue(tokens[2]);
        
        uint32_t result32 = static_cast<uint32_t>(val2) + static_cast<uint32_t>(val3);
        uint16_t result = static_cast<uint16_t>(std::min(result32, static_cast<uint32_t>(UINT16_MAX)));
        
        variables[var1] = result;
        
        return var1 + " = " + std::to_string(val2) + " + " + std::to_string(val3) + " = " + std::to_string(result);
    }
    return "";
}

std::string Process::processSubtract(const std::string& instruction) {
    size_t start = instruction.find('(') + 1;
    size_t end = instruction.find(')', start);
    std::string params = instruction.substr(start, end - start);
    
    std::vector<std::string> tokens;
    std::stringstream ss(params);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        tokens.push_back(token);
    }
    
    if (tokens.size() == 3) {
        std::string var1 = tokens[0];
        uint16_t val2 = getVariableOrValue(tokens[1]);
        uint16_t val3 = getVariableOrValue(tokens[2]);
        
        uint16_t result = (val2 > val3) ? (val2 - val3) : 0;
        
        variables[var1] = result;
        
        return var1 + " = " + std::to_string(val2) + " - " + std::to_string(val3) + " = " + std::to_string(result);
    }
    return "";
}

std::string Process::processPrint(const std::string& instruction) {
    size_t start = instruction.find('(') + 1;
    size_t end = instruction.find(')', start);
    std::string content = instruction.substr(start, end - start);
    
    if (content.find(" + ") != std::string::npos) {
        size_t plusPos = content.find(" + ");
        std::string msgPart = content.substr(0, plusPos);
        std::string varPart = content.substr(plusPos + 3);
        
        if (msgPart.front() == '"' && msgPart.back() == '"') {
            msgPart = msgPart.substr(1, msgPart.length() - 2);
        }
        
        uint16_t varValue = getVariableOrValue(varPart);
        return "OUTPUT: " + msgPart + std::to_string(varValue);
    } else {
        if (content.front() == '"' && content.back() == '"') {
            content = content.substr(1, content.length() - 2);
        }
        return "OUTPUT: " + content;
    }
}

std::string Process::processSleep(const std::string& instruction) {
    size_t start = instruction.find('(') + 1;
    size_t end = instruction.find(')', start);
    std::string ticksStr = instruction.substr(start, end - start);
    
    int ticks = std::stoi(ticksStr);
    sleepRemaining = ticks;
    state = ProcessState::WAITING;
    return "Sleeping for " + std::to_string(ticks) + " CPU ticks";
}

std::string Process::processFor(const std::string& instruction) {
    size_t repeatStart = instruction.rfind(',') + 1;
    size_t repeatEnd = instruction.find(')', repeatStart);
    std::string repeatsStr = instruction.substr(repeatStart, repeatEnd - repeatStart);
    repeatsStr.erase(std::remove_if(repeatsStr.begin(), repeatsStr.end(), ::isspace), repeatsStr.end());
    
    int repeats = std::stoi(repeatsStr);
    return "Executing FOR loop " + std::to_string(repeats) + " times";
}

uint16_t Process::getVariableOrValue(const std::string& token) {
    if (std::isdigit(token[0])) {
        int value = std::stoi(token);
        return static_cast<uint16_t>(std::max(0, std::min(value, static_cast<int>(UINT16_MAX))));
    } else {
        if (variables.find(token) == variables.end()) {
            variables[token] = 0;
        }
        return variables[token];
    }
}

bool Process::isComplete() const {
    return (pendingInstructions.empty() && executedInstructions >= totalInstructions) || 
           (state == ProcessState::TERMINATED);
}

void Process::updateMetrics() {
    int currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    if (state == ProcessState::TERMINATED) {
        turnaroundTime = currentTime - arrivalTime;
        waitingTime = turnaroundTime - burstTime;
        completionTimestamp = Utils::getCurrentTimestamp();
    }
}

Process::~Process() {
    variables.clear();
}

std::string Process::getStateString() const {
    switch (state) {
        case ProcessState::NEW: return "NEW";
        case ProcessState::READY: return "READY";
        case ProcessState::RUNNING: return "RUNNING";
        case ProcessState::WAITING: return "WAITING";
        case ProcessState::TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

void Process::setCustomInstructions(const std::vector<std::string>& instructions) {
    if (instructions.size() < 1 || instructions.size() > 50) {
        return;
    }
    
    totalInstructions = instructions.size();
    remainingTime = totalInstructions;
    burstTime = totalInstructions;
    
    while (!pendingInstructions.empty()) {
        pendingInstructions.pop();
    }
    
    for (const auto& instruction : instructions) {
        pendingInstructions.push(instruction);
    }
}

void Process::setMemoryAllocation(size_t memory) {
    allocatedMemory = memory;
}

void Process::handleMemoryViolation(uint32_t address) {
    memoryViolationOccurred = true;
    violationAddress = address;
    violationTimestamp = Utils::getCurrentTimestamp();
    state = ProcessState::TERMINATED;
}

bool Process::hasMemoryViolation() const {
    return memoryViolationOccurred;
}

std::string Process::getViolationInfo() const {
    if (!memoryViolationOccurred) return "";
    
    std::ostringstream oss;
    oss << "Process " << name << " shut down due to memory access violation error that occurred at " 
        << violationTimestamp << ". 0x" << std::hex << violationAddress << " invalid.";
    return oss.str();
}

std::string Process::processRead(const std::string& instruction) {
    std::istringstream iss(instruction);
    std::string command, hexAddr;
    
    if (!(iss >> command >> hexAddr)) {
        return "Invalid READ instruction format";
    }
    
    try {
        uint32_t address = std::stoul(hexAddr, nullptr, 16);
        
        if (address >= allocatedMemory) {
            handleMemoryViolation(address);
            return "Memory access violation at " + hexAddr;
        }
        
        int value = 0;
        
        return "READ " + hexAddr + " = " + std::to_string(value);
    } catch (const std::exception& e) {
        return "Invalid memory address: " + hexAddr;
    }
}

std::string Process::processWrite(const std::string& instruction) {
    std::istringstream iss(instruction);
    std::string command, hexAddr, valueStr;
    
    if (!(iss >> command >> hexAddr >> valueStr)) {
        return "Invalid WRITE instruction format";
    }
    
    try {
        uint32_t address = std::stoul(hexAddr, nullptr, 16);
        uint16_t value;
        
        if (variables.find(valueStr) != variables.end()) {
            value = variables[valueStr];
        } else {
            value = static_cast<uint16_t>(std::stoul(valueStr));
        }
        
        if (address >= allocatedMemory) {
            handleMemoryViolation(address);
            return "Memory access violation at " + hexAddr;
        }
        
        return "WRITE " + std::to_string(value) + " to " + hexAddr;
    } catch (const std::exception&) {
        return "Invalid WRITE parameters";
    }
}
