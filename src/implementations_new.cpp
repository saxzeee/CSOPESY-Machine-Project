#include "emulator.h"

Process::Process(const std::string& processName) 
    : name(processName), state(ProcessState::NEW), priority(0), 
      coreAssignment(-1), executedInstructions(0), totalInstructions(0) {
    
    static std::atomic<int> pidCounter{1};
    pid = "p" + std::string(3 - std::to_string(pidCounter.load()).length(), '0') + 
          std::to_string(pidCounter.fetch_add(1));
    
    creationTimestamp = Utils::getCurrentTimestamp();
    arrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void Process::generateInstructions(int count) {
    totalInstructions = count;
    remainingTime = count;
    burstTime = count;
    
    std::vector<std::string> instructionTypes = {
        "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV", 
        "CMP", "JMP", "CALL", "RET", "PUSH", "POP"
    };
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> typeDist(0, instructionTypes.size() - 1);
    std::uniform_int_distribution<> valueDist(1, 1000);
    
    for (int i = 0; i < count; ++i) {
        std::string instruction = instructionTypes[typeDist(gen)] + " " + 
                                std::to_string(valueDist(gen));
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
    std::string logEntry = "(" + timestamp + ") " + instruction;
    
    instructionHistory.push_back(logEntry);
    executedInstructions++;
    remainingTime--;
    
    if (responseTime == -1) {
        responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count() - arrivalTime;
    }
    
    return logEntry;
}

bool Process::isComplete() const {
    return pendingInstructions.empty() && executedInstructions >= totalInstructions;
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

namespace Utils {
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%m/%d/%Y %I:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        ss << std::put_time(std::localtime(&time_t), " %p");
        
        return ss.str();
    }
    
    void clearScreen() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
    
    void setTextColor(int color) {
        std::cout << "\033[" << color << "m";
    }
    
    void resetTextColor() {
        std::cout << "\033[0m";
    }
    
    std::string formatDuration(std::chrono::milliseconds duration) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto ms = duration - seconds;
        
        return std::to_string(seconds.count()) + "." + 
               std::to_string(ms.count()) + "s";
    }
    
    int generateRandomInt(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(min, max);
        return dist(gen);
    }
}
