#include "acevo/telemetry/streaming_trace.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"

// A row is written from inside engine jobs that may run on fibers. An SRW lock has no owning
// thread, and nothing is ever called while it is held, so a fiber that moves between threads
// around a row cannot trip it.
static SRWLOCK g_lock = SRWLOCK_INIT;
static std::string g_pending;
static HANDLE g_file = INVALID_HANDLE_VALUE;
static std::atomic<uint64_t> g_droppedRows{0};

// A stuck writer thread must not grow the buffer without end, a minute of a busy session is a
// few megabytes.
static const size_t kMaxPendingBytes = 64u * 1024u * 1024u;

bool TraceOn()
{
    return g_cfg.streamingTrace;
}

void TraceRow(const char* kind, const char* fmt, ...)
{
    if (!g_cfg.streamingTrace) return;

    char line[768];
    int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%.3f,%s,", NowSec(), kind);
    if (n < 0) return;
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof line - n - 2, fmt, ap);
    va_end(ap);
    if (m < 0) return;
    n += std::min(m, (int)(sizeof line - n - 3));
    line[n++] = '\r';
    line[n++] = '\n';

    AcquireSRWLockExclusive(&g_lock);
    if (g_pending.size() + n <= kMaxPendingBytes) g_pending.append(line, n);
    else g_droppedRows++;
    ReleaseSRWLockExclusive(&g_lock);
}

static void WriteOut(std::string& rows)
{
    if (rows.empty()) return;
    if (g_file == INVALID_HANDLE_VALUE) {
        std::wstring path = g_dir + L"acevo_perf_streaming.csv";
        g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_file == INVALID_HANDLE_VALUE) return;
        static const char header[] = "t_s,kind,a,b,c,d,e,f,g,h,i,j,k,l,m\r\n";
        DWORD w;
        WriteFile(g_file, header, (DWORD)(sizeof header - 1), &w, nullptr);
    }
    DWORD w;
    WriteFile(g_file, rows.data(), (DWORD)rows.size(), &w, nullptr);
}

void TraceFlush()
{
    if (!g_cfg.streamingTrace) return;

    std::string rows;
    AcquireSRWLockExclusive(&g_lock);
    rows.swap(g_pending);
    ReleaseSRWLockExclusive(&g_lock);

    WriteOut(rows);
}

// At process exit every other thread is already gone, and one of them may have died holding the
// lock, so the last rows are only written when the lock is free.
void TraceFinalFlush()
{
    if (!g_cfg.streamingTrace) return;

    std::string rows;
    if (!TryAcquireSRWLockExclusive(&g_lock)) return;
    rows.swap(g_pending);
    ReleaseSRWLockExclusive(&g_lock);

    WriteOut(rows);
    if (g_droppedRows.load()) Log("[trace] %llu streaming rows were dropped because the writer fell behind", (unsigned long long)g_droppedRows.load());
    if (g_file != INVALID_HANDLE_VALUE) CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
}
