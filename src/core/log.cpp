#include "acevo/core/log.h"

static HANDLE g_log = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCs;

void LogOpen(const std::wstring& path)
{
    InitializeCriticalSection(&g_logCs);
    g_log = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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
    EnterCriticalSection(&g_logCs);
    DWORD w; WriteFile(g_log, buf, (DWORD)n, &w, nullptr);
    LeaveCriticalSection(&g_logCs);
}

void LogClose()
{
    if (g_log == INVALID_HANDLE_VALUE) return;
    Log("detached");
    CloseHandle(g_log);
    g_log = INVALID_HANDLE_VALUE;
}
