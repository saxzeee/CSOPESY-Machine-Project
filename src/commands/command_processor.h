#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "../scheduler/scheduler.h"
#include <memory>
#include <map>
#include <functional>
#include <vector>
#include <string>

class CommandProcessor {
private:
    std::unique_ptr<Scheduler> scheduler;
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> commands;
    bool initialized = false;
    
    std::vector<std::string> parseCommand(const std::string& input);
    std::vector<std::string> parseInstructions(const std::string& instructionString);
    bool isValidMemorySize(size_t size);
    
public:
    CommandProcessor();
    void run();
    void displayHeader();
};

#endif
