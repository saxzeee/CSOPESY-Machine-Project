#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm> // for transform()

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

int main() {
    bool menuState = true;
    std::string inputCommand;
    std::map<std::string, ScreenSession> screens;

    dispHeader();

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
        else if (inputCommand.find("scheduler-start") != std::string::npos) {
            std::cout << "scheduler-start command recognized. Doing something.\n";
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
            std::cout <<    "Commands:\n" <<
                            "  initialize       - Initialize the processor configuration with ""config.txt"".\n" <<                                             // not implemented
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
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }

        inputCommand = "";
    }

    return 0;
}