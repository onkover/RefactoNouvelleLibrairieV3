#pragma once
#include <string>	
#include <iostream>

void SetConsoleMode();
int LoadGameDirectory(std::wstring nameVariable, std::wstring& output, const std::wstring defaultPath);
