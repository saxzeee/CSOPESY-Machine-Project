#include "../headers/process.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <random>
#include <atomic>

std::atomic<int> soloProcessCount(0);

bool readConfigFile(std::string filePath, schedConfig* config) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error opening config file\n";
        return false;
    }
    std::string fileLine;
    while (std::getline(file, fileLine)) {
        std::istringstream iss(fileLine);
        std::string attribute;
        if (!(iss >> attribute)) continue;

        if (attribute == "scheduler") {
            std::string schedValue;
            if (iss >> std::quoted(schedValue)) {
                config->algorithm = schedValue;
            }
            else {
                std::cerr << "Error reading algo\n";
            }
        }
        else {
            std::string configVals;
            if (!(iss >> configVals)) continue;
            if (attribute == "num-cpu") {
                config->numCores = std::stoi(configVals);
            }
            else if (attribute == "quantum-cycles") {
                config->quantumCycles = std::stoul(configVals);
            }
            else if (attribute == "batch-process-freq") {
                config->batchProcFreq = std::stoul(configVals);
            }
            else if (attribute == "min-ins") {
                config->minIns = std::stoul(configVals);
            }
            else if (attribute == "max-ins") {
                config->maxIns = std::stoul(configVals);
            }
            else if (attribute == "delay-per-exec") {
                config->delayPerExec = std::stoul(configVals);
            }
            else if (attribute == "max-overall-mem") {
                config->maxOverallMem = std::stoull(configVals);
            }
            else if (attribute == "mem-per-frame") {
                config->memPerFrame = std::stoull(configVals);
            }
            else if (attribute == "mem-per-proc") {
                config->memPerProc = std::stoull(configVals);
            }
        }
    }
    return true;
}

std::string getCurrentTimestamp() {
    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    std::ostringstream oss;
    oss << std::put_time(&ltm, "%m/%d/%Y, %I:%M:%S %p");
    return oss.str();
}

void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out, int nestLevel) {
    switch (instr.type) {
        case ICommand::InstrType::PRINT: {
            out << "PID " << proc.name << ": ";
            if (!instr.printVar.empty() && proc.variables.count(instr.printVar)) {
                out << instr.msg << " -> " << proc.variables[instr.printVar] << std::endl;
            } else {
                out << instr.msg << std::endl;
            }
            break;
        }
        case ICommand::InstrType::DECLARE: {
            proc.variables[instr.var1] = instr.value;
            out << "PID " << proc.name << ": DECLARE " << instr.var1 << " = " << instr.value << std::endl;
            break;
        }
        case ICommand::InstrType::ADD: {
            uint16_t v2 = proc.variables.count(instr.var2) ? proc.variables[instr.var2] : static_cast<uint16_t>(std::stoi(instr.var2));
            uint16_t v3 = proc.variables.count(instr.var3) ? proc.variables[instr.var3] : static_cast<uint16_t>(std::stoi(instr.var3));
            uint32_t sum = static_cast<uint32_t>(v2) + static_cast<uint32_t>(v3);
            if (sum > 65535) sum = 65535;
            proc.variables[instr.var1] = static_cast<uint16_t>(sum);
            out << "PID " << proc.name << ": ADD " << instr.var1 << " = " << v2 << " + " << v3 << " -> " << proc.variables[instr.var1] << std::endl;
            break;
        }
        case ICommand::InstrType::SUBTRACT: {
            uint16_t v2 = proc.variables.count(instr.var2) ? proc.variables[instr.var2] : static_cast<uint16_t>(std::stoi(instr.var2));
            uint16_t v3 = proc.variables.count(instr.var3) ? proc.variables[instr.var3] : static_cast<uint16_t>(std::stoi(instr.var3));
            int32_t diff = static_cast<int32_t>(v2) - static_cast<int32_t>(v3);
            if (diff < 0) diff = 0;
            proc.variables[instr.var1] = static_cast<uint16_t>(diff);
            out << "PID " << proc.name << ": SUBTRACT " << instr.var1 << " = " << v2 << " - " << v3 << " -> " << proc.variables[instr.var1] << std::endl;
            break;
        }
        case ICommand::InstrType::SLEEP: {
            out << "PID " << proc.name << ": SLEEP for " << (int)instr.sleepTicks << " ticks" << std::endl;
            if (proc.sleepRemaining == 0) { 
                proc.sleepRemaining = instr.sleepTicks;
            }
            break;
        }
        case ICommand::InstrType::FOR: {
            out << "PID " << proc.name << ": FOR loop - Nest Level: " 
                << nestLevel << ", Repeats: " << instr.repeats << std::endl;

            if (nestLevel >= 3) break; // max 3 nested loops 

            for (int i = 0; i < instr.repeats; ++i) {
                out << "PID " << proc.name << ": Iteration " << (i + 1) 
                    << " of " << instr.repeats << std::endl;

                for (const auto& subInstr : instr.body) {
                    executeInstruction(proc, subInstr, out, nestLevel + 1);
                }
            }
            break;
        }
    }
}

std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel) {
    std::vector<Instruction> instrs;
    std::vector<std::string> declaredVars;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> typeDist(0, nestLevel < 3 ? 5 : 4);
    std::uniform_int_distribution<> valueDist(1, 100);
    std::uniform_int_distribution<> sleepDist(1, 5);

    if (nestLevel == 0) { 
        Instruction xInit;
        xInit.type = ICommand::InstrType::DECLARE;
        xInit.var1 = "x";
        xInit.value = 0;
        instrs.push_back(xInit);
        declaredVars.push_back("x");
    }

    for (int i = 0; i < count; ++i) {
        Instruction instr;
        int t = typeDist(gen);

        switch (t) {
            case 0: { // PRINT 
                instr.type = ICommand::InstrType::PRINT;
                instr.msg = "Value from: " + procName;
                instr.printVar = "x"; 
                break;
            }

            case 1: { // DECLARE
                instr.type = ICommand::InstrType::DECLARE;
                instr.var1 = "v" + std::to_string(declaredVars.size());
                instr.value = valueDist(gen);
                declaredVars.push_back(instr.var1);
                break;
            }

            case 2: { // ADD 
                instr.type = ICommand::InstrType::ADD;
                instr.var1 = "x";
                instr.var2 = "x";
                instr.var3 = "1";
                break;
            }

            case 3: { // SUBTRACT
                if (declaredVars.empty()) continue;

                instr.type = ICommand::InstrType::SUBTRACT;
                instr.var1 = "v" + std::to_string(declaredVars.size());
                instr.var2 = declaredVars[gen() % declaredVars.size()];

                if (gen() % 2 == 0 && !declaredVars.empty()) {
                    instr.var3 = declaredVars[gen() % declaredVars.size()];
                } else {
                    instr.var3 = std::to_string(valueDist(gen));
                }

                declaredVars.push_back(instr.var1);
                break;
            }

            case 4: { // SLEEP
                instr.type = ICommand::InstrType::SLEEP;
                instr.sleepTicks = sleepDist(gen);
                break;
            }

            case 5: { // FOR
                if (nestLevel >= 3) continue; // prevent exceeding max nesting
                instr.type = ICommand::InstrType::FOR;
                instr.repeats = 2 + (gen() % 3); // repeat 2–4 times
                instr.body = generateRandomInstructions(procName, 2, nestLevel + 1);
                break;
            }
        }

        instrs.push_back(instr);
    }

    return instrs;
}
