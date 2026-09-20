#pragma once
#include "acevo/common.h"

// One log file next to the DLL, timestamped lines, safe from any thread.
// Opens `path`, and on failure `fallback`, saying in the file which one it took and why. A log
// nobody can open is silent about being silent, so the fallback is what makes it visible.
void LogOpen(const std::wstring& path, const std::wstring& fallback = L"");
void Log(const char* fmt, ...);
void LogClose();
