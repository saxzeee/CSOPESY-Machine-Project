#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm> // for transform()
// for fcfs implementation
#include <thread>
#include <queue>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

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

// Struct to hold screen session data
struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
};

// Get current time as formatted string
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

// process and print command
struct PrintCommand {
    int commandID;
};

struct Process {
    std::string name;
    std::queue<PrintCommand> printCommands;
    std::string createdTime;
    std::string finishedTime;
    int totalCommands;
    int finishedCommands;
    int assignedCore;
    bool finished;
    Process(const std::string& n, int numCommands)
        : name(n), createdTime(getCurrentTimestamp()), finishedTime(""), totalCommands(numCommands), finishedCommands(0), assignedCore(-1), finished(false)
    {
        for (int i = 0; i < numCommands; ++i) {
            printCommands.push({i+1});
        }
    }
};

// global
std::queue<Process*> readyQueue;
std::vector<Process*> runningProcesses;
std::vector<Process*> finishedProcesses;
std::mutex queueMutex;
std::condition_variable cv;
bool schedulerRunning = true;
const int NUM_CORES = 4;
std::vector<bool> coreBusy(NUM_CORES, false);

// worker thread
void cpuWorker(int coreID) {
    while (schedulerRunning) {
        Process* proc = nullptr;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [] { return !readyQueue.empty() || !schedulerRunning; });
            if (!schedulerRunning) break;
            if (!readyQueue.empty()) {
                proc = readyQueue.front();
                readyQueue.pop();
                proc->assignedCore = coreID;
                runningProcesses.push_back(proc);
                coreBusy[coreID] = true;
            }
        }
        if (proc) {
            std::ofstream ofs(proc->name + ".txt", std::ios::app);
            while (!proc->printCommands.empty()) {
                PrintCommand cmd = proc->printCommands.front();
                proc->printCommands.pop();
                proc->finishedCommands++;
                std::string timestamp = getCurrentTimestamp();
                ofs << "(" << timestamp << ") Core:" << coreID << " \"Hello world from " << proc->name << "!\"\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
            }
            ofs.close();
            proc->finished = true;
            proc->finishedTime = getCurrentTimestamp();
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                runningProcesses.erase(std::remove(runningProcesses.begin(), runningProcesses.end(), proc), runningProcesses.end());
                finishedProcesses.push_back(proc);
                coreBusy[coreID] = false;
            }
        }
    }
}

// scheduler
void schedulerThreadFunc() {
    while (schedulerRunning) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            cv.notify_all();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    bool menuState = true;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;

    dispHeader();

    // create 10 processes with 100 print commands
    std::vector<Process*> allProcesses;
    for (int i = 1; i <= 10; ++i) {
        std::string pname = "process" + std::string((i < 10 ? "0" : "")) + std::to_string(i);
        allProcesses.push_back(new Process(pname, 100));
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        for (auto* p : allProcesses) readyQueue.push(p);
    }

    // for starting scheduler
    std::thread schedulerThread(schedulerThreadFunc);
    std::vector<std::thread> cpuThreads;
    for (int i = 0; i < NUM_CORES; ++i) {
        cpuThreads.emplace_back(cpuWorker, i);
    }

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
            std::cout << "  initialize, screen -s <name>, screen -r <name>\n";
            std::cout << "  scheduler-test, scheduler-stop, report-util, clear, exit\n";
        }
        // screen -ls
        else if (inputCommand == "screen -ls") {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::cout << "---------------------------------------------\n";
            std::cout << "Running processes:\n";
            for (auto* p : runningProcesses) {
                std::cout << p->name << " (" << p->createdTime << ")  Core: " << p->assignedCore
                          << "   " << p->finishedCommands << " / " << p->totalCommands << "\n";
            }
            std::cout << "\nFinished processes:\n";
            for (auto* p : finishedProcesses) {
                std::cout << p->name << " (" << p->finishedTime << ")  Finished   "
                          << p->totalCommands << " / " << p->totalCommands << "\n";
            }
            std::cout << "---------------------------------------------\n";
        }
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }

        inputCommand = "";
    }

    // clean threads
    schedulerRunning = false;
    cv.notify_all();
    schedulerThread.join();
    for (auto& t : cpuThreads) t.join();
    for (auto* p : allProcesses) delete p;

    return 0;
}