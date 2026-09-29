#include "acevo/telemetry/streaming_trace.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"

// A row is written from inside engine jobs that may run on fibers. An SRW lock has no owning
// thread, and nothing is ever called while it is held, so a fiber that moves between threads
// around a row cannot trip it.
static SRWLOCK g_lock = SRWLOCK_INIT;
static std::string g_pending;
// The rows being written, traded with g_pending at each flush and cleared after, so the buffer the rows go
// into keeps the capacity it grew to. A fresh string each flush left the producers growing it again from
// nothing every second, each copy made under the exclusive lock every streamer and DirectStorage hook
// waits on. Only the flushes touch it, the timeline thread's and then the one at exit.
static std::string g_writing;
static HANDLE g_file = INVALID_HANDLE_VALUE;
static std::atomic<uint64_t> g_droppedRows{0};
// Set when the file cannot be made, which ends the trace's rows for the run. Retrying every second used to
// collect rows only to free them, and a file freed up mid session started partway with nothing to say so.
// The [writes] and repeated read lines that come with the switch carry on, since they are the log's own.
static std::atomic<bool> g_traceFailed{false};

// A stuck writer thread must not grow the buffer without end, a minute of a busy session is a
// few megabytes.
static const size_t kMaxPendingBytes = 64u * 1024u * 1024u;

bool TraceOn()
{
    return g_cfg.streamingTrace && !g_traceFailed.load(std::memory_order_relaxed);
}

void TraceRow(const char* kind, const char* fmt, ...)
{
    if (!TraceOn()) return;

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
    if (rows.empty() || g_traceFailed.load(std::memory_order_relaxed)) return;
    if (g_file == INVALID_HANDLE_VALUE) {
        std::wstring path = g_dir + L"acevo_perf_streaming.csv";
        g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_file == INVALID_HANDLE_VALUE) {
            g_traceFailed.store(true, std::memory_order_relaxed);
            Log("[trace] could not create acevo_perf_streaming.csv (error %lu), no streaming rows are written this run, "
                "the [writes] and repeated read lines carry on", GetLastError());
            return;
        }
        static const char header[] = "t_s,kind,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o\r\n";
        DWORD w;
        WriteFile(g_file, header, (DWORD)(sizeof header - 1), &w, nullptr);
    }
    DWORD w;
    WriteFile(g_file, rows.data(), (DWORD)rows.size(), &w, nullptr);
}

void TraceFlush()
{
    if (!g_cfg.streamingTrace) return;

    AcquireSRWLockExclusive(&g_lock);
    g_writing.swap(g_pending);
    ReleaseSRWLockExclusive(&g_lock);

    WriteOut(g_writing);
    g_writing.clear();
}

// At process exit every other thread is already gone, and one of them may have died holding the
// lock, so the last rows are only written when the lock is free.
void TraceFinalFlush()
{
    if (!g_cfg.streamingTrace) return;

    if (!TryAcquireSRWLockExclusive(&g_lock)) return;
    g_writing.swap(g_pending);
    ReleaseSRWLockExclusive(&g_lock);

    WriteOut(g_writing);
    g_writing.clear();
    if (g_droppedRows.load()) Log("[trace] %llu streaming rows were dropped because the writer fell behind", (unsigned long long)g_droppedRows.load());
    if (g_file != INVALID_HANDLE_VALUE) CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
}
