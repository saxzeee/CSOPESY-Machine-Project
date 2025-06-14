#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm> // for transform()
// for fcfs
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>

// Struct to hold screen session data
struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
};

// struct for process
struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
};

// global variables for process
std::vector<Process> processList;
std::vector<int> finishedProcesses;
std::mutex processMutex;
bool schedulerRunning = true;

// Cross-platform clear screen
void clearScreen() {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

// Utility to set terminal text color (ANSI)
void setTextColor(int colorID) {
    std::cout << "\033[" << colorID << "m";
}

void defaultColor() {
    std::cout << "\033[0m";
}

// ASCII Header Display
void dispHeader() {
    std::string opesy_ascii = R"(
     _/_/_/    _/_/_/    _/_/    _/_/_/    _/_/_/_/    _/_/_/  _/      _/  
  _/        _/        _/    _/  _/    _/  _/        _/          _/  _/     
 _/          _/_/    _/    _/  _/_/_/    _/_/_/      _/_/        _/        
_/              _/  _/    _/  _/        _/              _/      _/         
 _/_/_/  _/_/_/      _/_/    _/        _/_/_/_/  _/_/_/        _/                                                                               
)";
    setTextColor(34);
    std::cout << opesy_ascii << std::endl;
    setTextColor(32);
    std::cout << "Welcome to CSOPESY OS Emulator!\n";
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen.\n";
    defaultColor();
}

// get current timestamp
std::string getCurrentTimestamp() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(ltm, "%m/%d/%Y, %I:%M:%S %p");
    return oss.str();
}

// Display screen layout
void displayScreen(const ScreenSession &session) {
    clearScreen();
    setTextColor(36);
    std::cout << "Process name: " << session.name << "\n";
    std::cout << "Instruction: Line " << session.currentLine << " / " << session.totalLines << "\n";
    std::cout << "Created at: " << session.timestamp << "\n";
    defaultColor();
    std::cout << "\n(Type 'exit' to return to the main menu)\n";
}

// Loop for inside screen session
void screenLoop(ScreenSession &session) {
    std::string input;
    while (true) {
        displayScreen(session);
        std::cout << "\n>> ";
        std::getline(std::cin, input);

        if (input == "exit") {
            clearScreen();
            dispHeader();
            break;
        }

        // Simulate progressing through instructions
        if (session.currentLine < session.totalLines){
            session.currentLine++;
        }
    }
}

// CPU worker function
void cpuWorker(int coreID) {
    while (schedulerRunning) {
        processMutex.lock();
        int procIndex = -1;
        // Find the next unfinished process
        for (int i = 0; i < processList.size(); i++) {
            if (!processList[i].finished && processList[i].executedCommands < processList[i].totalCommands) {
                procIndex = i;
                break;
            }
        }
        processMutex.unlock();

        if (procIndex == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        processMutex.lock();
        Process &proc = processList[procIndex];
        proc.executedCommands++;
        int cmdNum = proc.executedCommands;
        if (proc.executedCommands == proc.totalCommands) {
            proc.finished = true;
            finishedProcesses.push_back(procIndex);
        }
        processMutex.unlock();

        std::string filename = proc.name + ".txt";
        std::ofstream outfile;
        outfile.open(filename, std::ios::app);
        outfile << "Print command " << cmdNum << " executed at " << getCurrentTimestamp()
                << " by CPU Core " << coreID << std::endl;
        outfile.close();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// scheduler
void scheduler() {
    while (true) {
        processMutex.lock();
        bool allDone = true;
        for (int i = 0; i < processList.size(); i++) {
            if (!processList[i].finished) {
                allDone = false;
                break;
            }
        }
        processMutex.unlock();
        if (allDone) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    schedulerRunning = false;
}

// show process status
void showScreenLS() {
    processMutex.lock();
    std::cout << "Running processes:\n";
    for (int i = 0; i < processList.size(); i++) {
        if (!processList[i].finished) {
            std::cout << "  " << processList[i].name << " (" << processList[i].executedCommands
                      << "/" << processList[i].totalCommands << ")\n";
        }
    }
    std::cout << "Finished processes:\n";
    for (int i = 0; i < processList.size(); i++) {
        if (processList[i].finished) {
            std::cout << "  " << processList[i].name << "\n";
        }
    }
    processMutex.unlock();
}

int main() {
    bool menuState = true;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;

    dispHeader();

    // create 10 processes with 100 print commands
    for (int i = 1; i <= 10; i++) {
        Process proc;
        proc.name = "process_" + std::to_string(i);
        proc.totalCommands = 100;
        proc.executedCommands = 0;
        proc.finished = false;
        processList.push_back(proc);
    }

    std::vector<std::thread> cpuThreads;
    for (int i = 1; i <= 4; i++) {
        cpuThreads.push_back(std::thread(cpuWorker, i));
    }

    std::thread schedThread(scheduler);

    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        // Convert to lowercase
        std::transform(inputCommand.begin(), inputCommand.end(), inputCommand.begin(), ::tolower);

        if (inputCommand.find("initialize") != std::string::npos) {
            std::cout << "initialize command recognized. Doing something.\n";
        }
        else if (inputCommand.rfind("screen -s ", 0) == 0) {
            std::string name = inputCommand.substr(10);
            if (screens.find(name) == screens.end()) {
                screens[name] = { name, 1, 10, getCurrentTimestamp() };
            }
            screenLoop(screens[name]);
        }
        else if (inputCommand.rfind("screen -r ", 0) == 0) {
            std::string name = inputCommand.substr(10);
            if (screens.find(name) != screens.end()) {
                screenLoop(screens[name]);
            } else {
                std::cout << "No screen session named '" << name << "' exists.\n";
            }
        }
        else if (inputCommand.find("scheduler-test") != std::string::npos) {
            std::cout << "scheduler-test command recognized. Doing something.\n";
        }
        else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            std::cout << "scheduler-stop command recognized. Doing something.\n";
        }
        else if (inputCommand.find("report-util") != std::string::npos) {
            std::cout << "report-util command recognized. Doing something.\n";
        }
        else if (inputCommand == "clear") {
            clearScreen();
            dispHeader();
        }
        else if (inputCommand == "exit") {
            menuState = false;
        }
        else if (inputCommand == "-help") {
            std::cout << "Available commands:\n";
            std::cout << "  initialize, screen -s <name>, screen -r <name>, screen -ls\n";
            std::cout << "  scheduler-test, scheduler-stop, report-util, clear, exit\n";
        }
        else if (inputCommand == "screen -ls") {
            showScreenLS();
        } 
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }

        inputCommand = "";
    }

    // stop scheduler
    schedulerRunning = false;
    for (int i = 0; i < cpuThreads.size(); i++) {
        cpuThreads[i].join();
    }
    schedThread.join();

    std::cout << "Emulator finished. Check the process text files for output.\n";
    return 0;
}