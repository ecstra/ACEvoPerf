#pragma once
#include "acevo/common.h"

// One log file next to the DLL, timestamped lines, safe from any thread.
void LogOpen(const std::wstring& path);
void Log(const char* fmt, ...);
void LogClose();
