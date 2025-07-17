#pragma once
#include <string>
#include <iostream>
#include "screen_session.h"
#include "scheduler.h"

void clearScreen();
void setTextColor(int colorID);
void defaultColor();
void dispHeader();
std::string getCurrentTimestamp();
void displayScreen(const ScreenSession& session);
void screenLoop(ScreenSession& session, Scheduler* scheduler);
void handleInitialize(schedConfig& config, std::unique_ptr<Scheduler>& scheduler, std::map<std::string, ScreenSession>& screens, bool& initialized);
void handleScreenS(const std::string& name, Scheduler* scheduler, std::map<std::string, ScreenSession>& screens);
void handleScreenR(const std::string& name, std::map<std::string, ScreenSession>& screens, Scheduler* scheduler);
void handleSchedulerStop(std::unique_ptr<Scheduler>& scheduler);
void handleHelp();
void executeInstruction(Process& proc, const Instruction& instr, std::ostream& out = std::cout, int nestLevel = 0);
std::vector<Instruction> generateRandomInstructions(const std::string& procName, int count, int nestLevel = 0);