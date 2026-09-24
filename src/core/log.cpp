#include "acevo/core/log.h"
#include "acevo/core/config.h"   // g_dir

std::wstring PublicPath(const std::wstring& path)
{
    if (!g_dir.empty() && path.size() >= g_dir.size()
        && _wcsnicmp(path.c_str(), g_dir.c_str(), g_dir.size()) == 0) {
        std::wstring rest = path.substr(g_dir.size());
        return rest.empty() ? L"the game folder" : rest;   // the path was the game folder itself
    }
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

static HANDLE g_log = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCs;
static std::atomic<bool> g_detaching{false};

// A thread killed inside Log at process exit leaves the lock held for good, so once detaching the lock is
// only taken when it is free, the way TraceFinalFlush takes its own, and a line that finds it held is
// dropped.
static bool EnterLog()
{
    if (!g_detaching) {
        EnterCriticalSection(&g_logCs);
        return true;
    }
    return TryEnterCriticalSection(&g_logCs) != FALSE;
}

// A path the player chose can be a folder, or sit under one that does not exist. Nothing could
// report that, because reporting goes through Log and Log is what just failed, so the player sees
// no file at all and reads it as the mod not loading. The fallback is the shipped name next to the
// DLL, which is where anyone looking for the log will look anyway.
void LogOpen(const std::wstring& path, const std::wstring& fallback)
{
    InitializeCriticalSection(&g_logCs);
    g_log = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log != INVALID_HANDLE_VALUE || fallback.empty() || fallback == path) return;

    DWORD err = GetLastError();
    g_log = CreateFileW(fallback.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log != INVALID_HANDLE_VALUE)
        Log("log: [log] file could not be opened at %ls (error %lu), writing here instead", PublicPath(path).c_str(), err);
}

void Log(const char* fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE) return;
    char buf[4096];
    SYSTEMTIME st; GetLocalTime(&st);
    int n = _snprintf_s(buf, sizeof buf, _TRUNCATE, "[%02d:%02d:%02d.%03d] ",
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    int m = vsnprintf(buf + n, sizeof buf - n - 3, fmt, ap);
    va_end(ap);
    if (m < 0) m = 0;
    if (m > (int)(sizeof buf - n - 3)) m = (int)(sizeof buf - n - 3);
    n += m;
    buf[n++] = '\r'; buf[n++] = '\n';
    if (!EnterLog()) return;
    DWORD w; WriteFile(g_log, buf, (DWORD)n, &w, nullptr);
    LeaveCriticalSection(&g_logCs);
}

void LogDetaching()
{
    g_detaching = true;
}

void LogClose()
{
    if (g_log == INVALID_HANDLE_VALUE) return;
    Log("detached");
    CloseHandle(g_log);
    g_log = INVALID_HANDLE_VALUE;
}
