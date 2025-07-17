#include "scheduler.h"
#include "utils.h"
#include "config.h"
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<int> soloProcessCount(0);

int main() {
    bool menuState = true;
    bool initialized = false;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;
    schedConfig config;
    dispHeader();
    std::unique_ptr<Scheduler> procScheduler;

    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        std::transform(inputCommand.begin(), inputCommand.end(), inputCommand.begin(), ::tolower);

        if (inputCommand.find("initialize") != std::string::npos) {
            handleInitialize(config, procScheduler, screens, initialized);
        } else if (inputCommand == "help") {
            handleHelp();
        } else if (inputCommand == "clear") {
            clearScreen();
            dispHeader();
        } else if (inputCommand == "exit") {
            if (procScheduler && procScheduler->isRunning()) {
                std::cout << "Stopping scheduler before exit...\n";
                procScheduler->shutdown();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            while (soloProcessCount > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            menuState = false;
            procScheduler.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else if (!initialized) {
            std::cout << "Run the initialize command first before proceeding.\n";
        } else if (inputCommand.rfind("screen -s ", 0) == 0) {
            handleScreenS(inputCommand.substr(10), procScheduler.get(), screens);
        } else if (inputCommand.rfind("screen -r ", 0) == 0) {
            handleScreenR(inputCommand.substr(10), screens, procScheduler.get());
        } else if (inputCommand.find("screen -ls") != std::string::npos) {
            procScheduler->showScreenLS();
        } else if (inputCommand.find("scheduler-start") != std::string::npos) {
            procScheduler->startScheduler();
        } else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            handleSchedulerStop(procScheduler);
        } else if (inputCommand.find("report-util") != std::string::npos) {
            if (procScheduler) {
                procScheduler->generateLog("csopesy-log.txt");
                procScheduler->printMemory();
            } else {
                std::cout << "Scheduler not initialized.\n";
            }
        } else if (inputCommand == "mem-status") {
            if (procScheduler) {
                procScheduler->printMemory();
            } else {
                std::cout << "Scheduler not initialized.\n";
            }
        } else {
            std::cout << "Command not recognized. Type 'help' to display commands.\n";
        }
        inputCommand = "";
    }
    
    return 0;
}