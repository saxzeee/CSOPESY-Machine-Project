#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <chrono>

namespace Utils {
    std::string getCurrentTimestamp();
    void clearScreen();
    void setTextColor(int color);
    void resetTextColor();
    std::string formatDuration(std::chrono::milliseconds duration);
    int generateRandomInt(int min, int max);
}

#endif
