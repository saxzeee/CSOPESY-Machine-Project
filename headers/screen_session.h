#pragma once
#include <string>
#include <vector>

struct ScreenSession {
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;
    std::vector<std::string> sessionLog;
    std::vector<std::string> processLogs;
};