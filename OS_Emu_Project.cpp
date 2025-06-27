
#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>

// Struct to hold screen session data
struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
};

// Struct for process -> refactor into class? 
struct Process {
    std::string name;
    int totalCommands;
    int executedCommands;
    bool finished;
    std::string finishTimestamp;
};

// Struct to hold config.txt details
struct schedConfig {
    unsigned int numCores;
    std::string algorithm;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;

};

class Scheduler {
private:
    std::vector<Process> processList;
    std::vector<int> finishedProcesses;
    std::mutex processMutex;
    bool schedulerRunning = false;
    std::thread schedulerMain;
    std::map<std::string, ScreenSession>* screenSessions = nullptr;

    void cpuWorker(int coreID) {
        while (true) {
            processMutex.lock();
            int procIndex = -1;
            for (size_t i = 0; i < processList.size(); i++) {
                if (!processList[i].finished && processList[i].executedCommands < processList[i].totalCommands) {
                    procIndex = i;
                    break;
                }
            }
            processMutex.unlock();

            if (procIndex == -1) {
                if (!schedulerRunning) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Copy data we need outside the lock to avoid holding it during I/O
            std::string procName;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                Process& proc = processList[procIndex];

                // Log before increment
                std::string filename = proc.name + ".txt";
                std::ofstream outfile(filename, std::ios::app);
                outfile << "(" << getCurrentTimestamp() << ") "
                    << "Core:" << (coreID - 1) << " "
                    << "\"Hello world from " << proc.name << "!\"" << std::endl;
                outfile.close();

                proc.executedCommands++;
                procName = proc.name;

                if (screenSessions) {
                    auto it = screenSessions->find(proc.name);
                    if (it != screenSessions->end()) {
                        it->second.currentLine = proc.executedCommands;
                    }
                }

                if (proc.executedCommands == proc.totalCommands) {
                    proc.finished = true;
                    proc.finishTimestamp = getCurrentTimestamp();
                    finishedProcesses.push_back(procIndex);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // edit
    void scheduler() {
        while (true) {
            processMutex.lock();
            bool allDone = true;
            for (size_t i = 0; i < processList.size(); i++) {
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

    std::string getCurrentTimestamp() {
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        std::ostringstream oss;
        oss << std::put_time(&ltm, "%m/%d/%Y, %I:%M:%S %p");
        return oss.str();
    }

public:
    unsigned int numCores;
    std::string algorithm;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;

	Scheduler(const schedConfig& config) {
	    numCores = config.numCores;
	    algorithm = config.algorithm;
	    quantumCycles = config.quantumCycles;
	    batchProcFreq = config.batchProcFreq;
	    minIns = config.minIns;
	    maxIns = config.maxIns;
	    delayPerExec = config.delayPerExec;
	}

    void addProcess(const Process& process) {
        processList.push_back(process);
    }

    void startScheduler() {
        if (schedulerRunning) {
            std::cout << "Scheduler already running.\n";
            return;
        }
        schedulerRunning = true;
        schedulerMain = std::thread(&Scheduler::runScheduler, this);
    }

    void runScheduler() {
        // create 10 dummy processes with 100 print commands (for testing)
        for (int i = 1; i <= 10; i++) {
            Process proc;
            proc.name = "process" + std::to_string(i);
            proc.totalCommands = 100;
            proc.executedCommands = 0;
            proc.finished = false;
            processList.push_back(proc);

            if (screenSessions) {
                std::lock_guard<std::mutex> lock(processMutex);
                (*screenSessions)[proc.name] = {
                    proc.name, 1, proc.totalCommands, getCurrentTimestamp()
                };
            }
        }

        std::vector<std::thread> cpuThreads;
        for (int i = 1; i <= static_cast<int>(numCores); i++) {
            cpuThreads.push_back(std::thread(&Scheduler::cpuWorker, this, i));
        }

        std::thread schedThread(&Scheduler::scheduler, this);

        schedThread.join();
		for (size_t i = 0; i < cpuThreads.size(); i++) {
		    cpuThreads[i].join();
		}

    }
    void printConfig() const { // display config for verifying if it creates and assigns attributes correctly
        std::cout << "---- Scheduler Configuration ----\n";
        std::cout << "Number of CPU Cores   : " << numCores << "\n";
        std::cout << "Scheduling Algorithm  : " << algorithm << "\n";
        std::cout << "Quantum Cycles        : " << quantumCycles << "\n";
        std::cout << "Batch Process Freq    : " << batchProcFreq << "\n";
        std::cout << "Min Instructions      : " << minIns << "\n";
        std::cout << "Max Instructions      : " << maxIns << "\n";
        std::cout << "Delay per Execution   : " << delayPerExec << "\n";
        std::cout << "----------------------------------\n";
    }

    // show process status
    void showScreenLS() {
        processMutex.lock();
        std::cout << std::left; // Align text to the left
        int nameWidth = 12;

        std::cout << "---------------------------------------------\n";
        std::cout << "Running processes:\n";
        for (size_t i = 0; i < processList.size(); i++) {
            if (!processList[i].finished) {
                std::cout << std::setw(nameWidth) << processList[i].name << "  ";
                std::cout << "(" << getCurrentTimestamp() << ")  ";
                std::cout << "Core: " << (i % 4) << "  ";
                std::cout << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
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
    void setScreenMap(std::map<std::string, ScreenSession>* screens) {
        screenSessions = screens;
     }
    
};
// global variables for process
std::vector<Process> processList;
std::vector<int> finishedProcesses;
std::mutex processMutex;

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
    tm ltm;
    localtime_s(&ltm, &now);
    std::ostringstream oss;
    oss << std::put_time(&ltm, "%m/%d/%Y, %I:%M:%S %p");
    return oss.str();
}

// Display screen layout
void displayScreen(const ScreenSession& session) {
    clearScreen();
    setTextColor(36);
    std::cout << "Process name: " << session.name << "\n";
    std::cout << "Instruction: Line " << session.currentLine << " / " << session.totalLines << "\n";
    std::cout << "Created at: " << session.timestamp << "\n";
    defaultColor();
    std::cout << "\n(Type 'exit' to return to the main menu)\n";
}

// Loop for inside screen session
void screenLoop(ScreenSession& session) {
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
		else if (input == "process-smi") {
		    std::string logFile = session.name + ".txt";
		    std::ifstream log(logFile);
		    if (log.is_open()) {
		        std::cout << "===== PROCESS INFO: " << session.name << " =====\n";
		        std::string line;
		        while (std::getline(log, line)) {
		            std::cout << line << "\n";
		        }
		        log.close();
		    } else {
		        std::cout << "No logs found for this process.\n";
		    }
		
		    // Check if finished
		    for (const auto& proc : processList) {
		        if (proc.name == session.name && proc.finished) {
		            std::cout << "Finished!\n";
		        }
		    }
		
		    // Pause before refresh
		    std::cout << "\nPress Enter to continue...";
		    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		}
        else {
            std::cout << "Unknown command.\n";
        }
    }
}


bool readConfigFile(std::string filePath, schedConfig* config) { //read the configs and edit the pased config struct that holds the details
    std::ifstream file(filePath);
    if (!file.is_open()) { // simple error cheking
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
        }
    }
    return true;
}

void handleInitialize(schedConfig& config, std::unique_ptr<Scheduler>& scheduler, std::map<std::string, ScreenSession>& screens, bool& initialized) {
    if (readConfigFile("config.txt", &config)) {
        scheduler = std::make_unique<Scheduler>(config);
        scheduler->printConfig();
        scheduler->setScreenMap(&screens);
        initialized = true;
    } else {
        std::cout << "Initialization failed.\n";
    }
}

void handleScreenS(const std::string& name, Scheduler* scheduler, std::map<std::string, ScreenSession>& screens) {
    bool found = false, finished = false;
    for (const auto& proc : processList) {
        if (proc.name == name) {
            found = true;
            finished = proc.finished;
            break;
        }
    }

    if (finished) {
        std::cout << "Process '" << name << "' has already finished.\n";
        return;
    }

    if (found) {
        screenLoop(screens[name]);
        return;
    }

    // Create new process
    Process proc;
    proc.name = name;
    proc.totalCommands = 10;  // Replace with randomized logic if needed
    proc.executedCommands = 0;
    proc.finished = false;

    screens[name] = { name, 1, proc.totalCommands, getCurrentTimestamp() };

    std::ofstream logFile(name + ".txt", std::ios::trunc);
    logFile << "Process name: " << name << "\nLogs:\n\n";
    logFile.close();

    scheduler->addProcess(proc);
    screenLoop(screens[name]);
}

void handleScreenR(const std::string& name, std::map<std::string, ScreenSession>& screens) {
    auto it = screens.find(name);
    if (it == screens.end()) {
        std::cout << "Process " << name << " not found.\n";
        return;
    }

    for (const auto& proc : processList) {
        if (proc.name == name && proc.finished) {
            std::cout << "Process " << name << " has already finished.\n";
            return;
        }
    }

    screenLoop(it->second);
}

void handleSchedulerStop() {
    std::cout << "scheduler-stop command recognized. (Not implemented)\n";
}

void handleReportUtil() {
    std::cout << "report-util command recognized. (Not implemented)\n";
}

void handleHelp() {
    std::cout << "Commands:\n"
              << "  initialize       - Initialize the processor configuration with \"config.txt\".\n"
              << "  screen -s <name> - Attach or create a screen session for a process.\n"
              << "  screen -r <name> - Resume an existing screen session if still running.\n"
              << "       process-smi - Show process info inside screen.\n"
              << "       exit        - Exit the screen session.\n"
              << "  screen -ls       - Show current CPU/process usage.\n"
              << "  scheduler-start  - Start dummy process generation.\n"
              << "  scheduler-stop   - Stop process generation (not yet implemented).\n"
              << "  report-util      - Save CPU utilization report to file.\n"
              << "  clear            - Clear the screen.\n"
              << "  exit             - Exit the emulator.\n";
}

int main() {
    bool menuState = true;
    bool initialized = false;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;
    schedConfig config;
    dispHeader();
    std::unique_ptr<Scheduler> procScheduler; //unique ptr for process scheduler where it will be created & config will be assigned later, also unique_ptr for memory management
    // for the headers
    /* 
    for (auto& proc : processList) {
        std::string filename = proc.name + ".txt";
        std::ofstream outfile(filename, std::ios::trunc);
        outfile << "Process name: " << proc.name << std::endl;
        outfile << "Logs:" << std::endl << std::endl;
        outfile.close();
    } 

    std::vector<std::thread> cpuThreads;
    for (int i = 1; i <= 4; i++) {
        cpuThreads.push_back(std::thread(cpuWorker, i));
    }

    std::thread schedThread(scheduler);
    */
    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        // Convert to lowercase
        std::transform(inputCommand.begin(), inputCommand.end(), inputCommand.begin(), ::tolower);

        if (inputCommand.find("initialize") != std::string::npos) {
            // initialize -> read config.txt file and setup scheduler details using the given config
            handleInitialize(config, procScheduler, screens, initialized);
        }
        else if (inputCommand == "-help") {
            handleHelp();
        }
        else if (inputCommand == "clear") {
            clearScreen();
            dispHeader();
        }
        else if (inputCommand == "exit") {
            menuState = false;
            /*
            std::cout << "Waiting for all processes to finish...\n";
            schedThread.join();
            for (int i = 0; i < cpuThreads.size(); i++) {
                cpuThreads[i].join();
            }
            std::cout << "All processes finished. Exiting emulator.\n";
            */
        }
        else if (!initialized) {
            std::cout << "Run the initialize command first before proceeding.\n";
        }
        else if (inputCommand.rfind("screen -s ", 0) == 0) {
            handleScreenS(inputCommand.substr(10), procScheduler.get(), screens);
        }
        else if (inputCommand.rfind("screen -r ", 0) == 0) {
            handleScreenR(inputCommand.substr(10), screens);
        }
        else if (inputCommand.find("screen -ls") != std::string::npos) {
            procScheduler->showScreenLS();
        }
        else if (inputCommand.find("scheduler-start") != std::string::npos) {
            procScheduler->startScheduler();
        }
        else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            handleSchedulerStop();
        }
        else if (inputCommand.find("report-util") != std::string::npos) {
            handleReportUtil();
        }
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }
        inputCommand = "";
    }
    return 0;
}