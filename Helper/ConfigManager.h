#pragma once
#include <string>	
#include "../config.h"

void InitConsole();
void ShutdownConsole();
bool ProgrammeConfig(const std::string& path, config& cfg);