#pragma once
#include "acevo/common.h"

// One log file next to the DLL, timestamped lines, safe from any thread.
// Opens `path`, and on failure `fallback`, saying in the file which one it took and why. A log
// nobody can open is silent about being silent, so the fallback is what makes it visible.
void LogOpen(const std::wstring& path, const std::wstring& fallback = L"");
void Log(const char* fmt, ...);
void LogClose();

// A path with the machine taken out of it. The readme tells players to attach this log to a public
// report, and a Steam library under a user profile, or a save folder which is always under one,
// puts that player's own name in every absolute path the mod writes.
//
// A path under the game folder keeps the part that says which file and which subfolder. Anything
// else, and that includes the game's own save files which are the highest volume path in the log
// by far, comes back as its file name alone. Losing the subfolder there is the point rather than a
// shortfall, since the subfolder is the profile.
//
// Every path that reaches Log goes through this. A new Log call with a raw path in it puts F-07
// back, and nothing but this comment will catch that.
std::wstring PublicPath(const std::wstring& path);
