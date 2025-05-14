
#include <iostream>
#include <string>
//commands: initialize, screen, scheduler-test, scheduler-stop, report-util, clear, and exit.

void setTextColor(int colorID)
{
    std::cout << "\033[" << colorID << "m";
}

void defaultColor() { 
    std::cout << "\033[0m"; 
}

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
    std::cout << "Welcome to CSOPESY OS Emulator! \n";
    std::cout << R"(Type 'exit' to quit, 'clear' to clear the screen.)" << std::endl;
    defaultColor();
}
int main()
{
    bool menuState = true;
    std::string inputCommand;
    dispHeader();

    while (menuState) {
        std::cout << "\nEnter a command: ";
        std::getline(std::cin, inputCommand);

        if (inputCommand.find("initialize") != std::string::npos) {
            std::cout << "initialize command recognized. Doing something. \n";
            // do soemthign
        }
        else if (inputCommand.find("screen") != std::string::npos) {
            std::cout << "screen command recognized. Doing something. \n";
            // do something
        }
        else if (inputCommand.find("scheduler-test") != std::string::npos) {
            std::cout << "scheduler-test command recognized. Doing something. \n";
            // do something
        }
        else if (inputCommand.find("scheduler-stop") != std::string::npos) {
            std::cout << "scheduler-stop command recognized. Doing something. \n";
            // something do
        }
        else if (inputCommand.find("report-util") != std::string::npos) {
            std::cout << "report-util command recognized. Doing something. \n";
            // dothing some
        }
        else if (inputCommand == "clear") {
            system("cls"); //i wanted to avoid but the escape sequence doesnt clear
            //std::cout << "\033[2J\033[1;1H"; //not really clearing
            dispHeader();
        }
        else if (inputCommand == "exit") {
            menuState = false;
        }
        else if (inputCommand == "-help") {
            std::cout << "initialize, screen, scheduler-test, scheduler-stop, report-util, clear, and exit.\n";
        }
        else {
            std::cout << "Command not recognized. -help to display commands\n";
        }
        inputCommand = ""; //clear string for next commabnd
    }
    return 0;
}











//keeping this here for now for navigation tips haha

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
