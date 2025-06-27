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

class ProcessTest {
public:
    struct RequirementFlags {
        bool requireFiles;
        int numFiles;
        bool requireMemory;
        int memoryRequired;

    };
    enum processState {
        Ready,
        Running,
        Waiting,
        Finished,

    };

    enum DataType {
        Integer,
        Float,
        Double,
        String
    };

    struct Symbol {
        DataType dtype;
        std::string value;
    };

private:
    int pid;
    std::string name;
    RequirementFlags requirements;
    processState state;
};



// Base instruction interface
class ICommand {
public:
    enum CommandType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };
    ICommand(int pid, CommandType type) : pid(pid), commandType(type) {}
    virtual ~ICommand() = default;
    CommandType getCommandType() { return commandType; }
    virtual void execute() = 0;

protected:
    int pid;
    CommandType commandType;
};

// PRINT command
class PrintCommand : public ICommand {
public:
    PrintCommand(int pid, const std::string& toPrint) : ICommand(pid, PRINT), toPrint(toPrint) {}

    void execute() override {
        // Simulate printing to the process's screen/log
        std::cout << "PID " << pid << ": " << toPrint << std::endl;
    }

private:
    std::string toPrint;
};

// DECLARE command
class DeclareCommand : public ICommand {
public:
    DeclareCommand(int pid, const std::string& varName, uint16_t value)
        : ICommand(pid, DECLARE), varName(varName), value(value) {}

    void execute() override {
        // Simulate variable declaration (store in process memory)
        std::cout << "PID " << pid << ": DECLARE " << varName << " = " << value << std::endl;
        // TODO: Actually store in process symbol table
    }

private:
    std::string varName;
    uint16_t value;
};

// ADD command
class AddCommand : public ICommand {
public:
    AddCommand(int pid, const std::string& dest, const std::string& src1, const std::string& src2)
        : ICommand(pid, ADD), dest(dest), src1(src1), src2(src2) {}

    void execute() override {
        // Simulate addition (fetch from symbol table, add, store result)
        std::cout << "PID " << pid << ": ADD " << dest << " = " << src1 << " + " << src2 << std::endl;
        // TODO: Actually perform addition and store in symbol table
    }

private:
    std::string dest, src1, src2;
};

// SUBTRACT command
class SubtractCommand : public ICommand {
public:
    SubtractCommand(int pid, const std::string& dest, const std::string& src1, const std::string& src2)
        : ICommand(pid, SUBTRACT), dest(dest), src1(src1), src2(src2) {}

    void execute() override {
        std::cout << "PID " << pid << ": SUBTRACT " << dest << " = " << src1 << " - " << src2 << std::endl;
        // TODO: Actually perform subtraction and store in symbol table
    }

private:
    std::string dest, src1, src2;
};

// SLEEP command
class SleepCommand : public ICommand {
public:
    SleepCommand(int pid, uint8_t ticks)
        : ICommand(pid, SLEEP), ticks(ticks) {}

    void execute() override {
        std::cout << "PID " << pid << ": SLEEP for " << (int)ticks << " ticks" << std::endl;
        // TODO: Actually implement sleep logic in scheduler
    }

private:
    uint8_t ticks;
};

// FOR command (simplified)
class ForCommand : public ICommand {
public:
    ForCommand(int pid, std::vector<ICommand*> body, int repeats)
        : ICommand(pid, FOR), body(body), repeats(repeats) {}

    void execute() override {
        std::cout << "PID " << pid << ": FOR loop x" << repeats << std::endl;
        for (int i = 0; i < repeats; ++i) {
            for (auto* cmd : body) {
                cmd->execute();
            }
        }
    }

private:
    std::vector<ICommand*> body;
    int repeats;
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
            for (int i = 0; i < processList.size(); i++) {
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
            bool justFinished = false;
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
                    justFinished = true;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    // ediot
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
        for (int i = 1; i <= numCores; i++) {
            cpuThreads.push_back(std::thread(&Scheduler::cpuWorker, this, i));
        }

        std::thread schedThread(&Scheduler::scheduler, this);

        schedThread.join();
        for (int i = 0; i < cpuThreads.size(); i++) {
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
        for (int i = 0; i < processList.size(); i++) {
            if (!processList[i].finished) {
                std::cout << std::setw(nameWidth) << processList[i].name << "  ";
                std::cout << "(" << getCurrentTimestamp() << ")  ";
                std::cout << "Core: " << (i % 4) << "  ";
                std::cout << processList[i].executedCommands << " / " << processList[i].totalCommands << "\n";
            }
        }
        std::cout << "\nFinished processes:\n";
        for (int i = 0; i < processList.size(); i++) {
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

//bool schedulerRunning = true;

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

        // Simulate progressing through instructions
        if (session.currentLine < session.totalLines) {
            session.currentLine++;
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














int main() {
    bool menuState = true;
    bool initialized = false;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;
    schedConfig config;
    dispHeader();
    std::unique_ptr<Scheduler> procScheduler; //unique ptr for process scheduler where it will be created & config will be assigned later, also unique_ptr for memory management

    // Place this in main(), before your main loop, for quick testing

    // Example: Test each instruction
    
    std::vector<ICommand*> testInstructions;
    testInstructions.push_back(new PrintCommand(1, "Hello world from test process!"));
    testInstructions.push_back(new DeclareCommand(1, "x", 42));
    testInstructions.push_back(new AddCommand(1, "sum", "x", "10"));
    testInstructions.push_back(new SubtractCommand(1, "diff", "x", "5"));
    testInstructions.push_back(new SleepCommand(1, 3));

    // FOR loop with 2 repeats of a print
    std::vector<ICommand*> forBody;
    forBody.push_back(new PrintCommand(1, "Inside FOR loop!"));
    testInstructions.push_back(new ForCommand(1, forBody, 2));

    // Execute all
    for (auto* cmd : testInstructions) {
        cmd->execute();
    }

    // Cleanup
    for (auto* cmd : testInstructions) delete cmd;
    for (auto* cmd : forBody) delete cmd;
    

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
            readConfigFile("config.txt", &config); 
            procScheduler = std::make_unique<Scheduler>(config);
            procScheduler->printConfig();
            procScheduler->setScreenMap(&screens);
            initialized = true;
        }
        else if (inputCommand == "-help") {
            std::cout << "Commands:\n" <<
                "  initialize       - Initialize the processor configuration with ""config.txt"".\n" <<                                             
                "  screen -s <name> - Start a new screen session with a given name.\n" <<
                "  screen -r <name> - Resume an existing screen session with the given name.\n" <<
                "       process-smi - Prints information about the process (upon entering ""screen -s/-r"" command).\n" <<                          // not implemented
                "       exit        - Exit the screen session.\n" <<
                "  screen -ls       - Display CPU utilization, cores used, cores, available, and summary of running and finished processes" <<      // not implemented
                "  scheduler-start  - Generate a batch of dummy processes for the CPU scheduler.\n" <<                                              // not implemented
                "  scheduler-stop   - Stop generating dummy processes.\n" <<                                                                        // not implemented
                "  report-util      - Generate a report of CPU utilization.\n" <<                                                                   // not implemented      
                "  clear            - Clear the screen.\n" <<
                "  exit             - Exit the emulator.\n";
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
            }
            else {
                std::cout << "No screen session named '" << name << "' exists.\n";
            }
        }
        else if (inputCommand.find("screen -ls") != std::string::npos) {
            procScheduler->showScreenLS();
        }
        else if (inputCommand.find("scheduler-start") != std::string::npos) {
            procScheduler->startScheduler();

        }
        else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            std::cout << "scheduler-stop command recognized. Doing something.\n";
        }
        else if (inputCommand.find("report-util") != std::string::npos) {
            std::cout << "report-util command recognized. Doing something.\n";
        }
        
        
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }

        inputCommand = "";
    }
    return 0;
}