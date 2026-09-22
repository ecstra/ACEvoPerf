#pragma once
#include "acevo/common.h"

// One log file next to the DLL, timestamped lines, safe from any thread.
// Opens `path`, and on failure `fallback`, saying in the file which one it took and why. A log
// nobody can open is silent about being silent, so the fallback is what makes it visible.
void LogOpen(const std::wstring& path, const std::wstring& fallback = L"");
void Log(const char* fmt, ...);
void LogClose();

// A path with the machine taken out of it. The readme tells players to attach this log to a public
// report, and a Steam library under a user profile puts that player's own name in every absolute
// path the mod writes. Everything the mod logs sits under the game folder, so what survives is the
// part that says which file and which subfolder, which is all the diagnostic value there was.
// A path outside the game folder comes back as its file name alone.
std::wstring PublicPath(const std::wstring& path);
