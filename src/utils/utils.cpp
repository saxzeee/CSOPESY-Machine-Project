#include "utils.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

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
