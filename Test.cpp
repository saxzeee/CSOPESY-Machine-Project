#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>

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
    system("cls");
    setTextColor(36);
    std::cout << "=== Screen Session: " << session.name << " ===\n";
    defaultColor();
    std::cout << "Process name: " << session.name << "\n";
    std::cout << "Instruction: Line " << session.currentLine << " / " << session.totalLines << "\n";
    std::cout << "Created at: " << session.timestamp << "\n";
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
        	system("cls");
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
        for (char &c : inputCommand) {
            if (c >= 'A' && c <= 'Z') {
                c += 32;
            }
        }

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
            system("cls");
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
        else {
            std::cout << "Command not recognized. Type '-help' to display commands.\n";
        }

        inputCommand = "";
    }

    return 0;
}
