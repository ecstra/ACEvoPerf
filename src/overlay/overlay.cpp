// Package override layer
//
// Loose files under <game>\acevo_mods\<package path> shadow entries of
// content.kspkg. The engine reads the package's table of contents (the last
// 64 MB of the file) with ordinary file I/O at startup and then streams every
// entry as one DirectStorage request whose offset and size come from that
// table. So two interceptions are enough:
//   1. ReadFile on the package handle: the table range is answered from a
//      modified copy in which overridden entries carry the loose file's size
//      and an offset past the end of the package (a "virtual" offset).
//   2. DirectStorage requests whose offset is virtual are pointed at an
//      IDStorageFile opened on the loose file.
// The package on disk is never written.
#include "acevo/overlay/overlay.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"
#include "acevo/dstorage/stats.h"
#include "acevo/dstorage/proxy.h"

namespace overlay {

static const BYTE XOR_KEY[8] = { 0xC1, 0x35, 0x11, 0x7D, 0xA9, 0x21, 0x97, 0x9F };
static const uint64_t SLOT = 256;
static const uint64_t VIRT_ALIGN = 65536;

struct Override {
    std::wstring loosePath;     // full path of the loose file
    std::string  pkgPath;       // package path, lower case, backslashes
    uint64_t     size = 0;
    uint64_t     virtOffset = 0;
    IDStorageFile* dsFile = nullptr;
    bool         inserted = false;  // true when the package had no entry of that name
};

static std::vector<Override> g_files;
static std::vector<BYTE> g_toc;          // modified table, XOR encoded like the original
static uint64_t g_pkgSize = 0, g_tocStart = 0, g_tocSize = 0, g_virtBase = 0, g_virtEnd = 0;
static bool g_tocBuilt = false, g_tocFailed = false, g_active = false;
static CRITICAL_SECTION g_cs;
static std::vector<HANDLE> g_pkgHandles;
static std::atomic<uint64_t> g_redirected{0};
static int g_traceLines = 0;

typedef HANDLE (WINAPI *PFN_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *PFN_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *PFN_CreateFile2)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
typedef BOOL   (WINAPI *PFN_ReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL   (WINAPI *PFN_SetFilePointerEx)(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);
typedef BOOL   (WINAPI *PFN_CloseHandle)(HANDLE);
static PFN_CreateFileW g_origCreateFileW; static PFN_CreateFileA g_origCreateFileA; static PFN_CreateFile2 g_origCreateFile2;
static PFN_ReadFile g_origReadFile; static PFN_SetFilePointerEx g_origSetFilePointerEx; static PFN_CloseHandle g_origCloseHandle;

static uint64_t Fnv1a64Utf16(const std::string& path)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : path) {
        h = (h ^ c) * 0x100000001b3ull;      // low byte of the UTF-16 code unit
        h = (h ^ 0) * 0x100000001b3ull;      // high byte, always zero for ASCII paths
    }
    return h;
}

// The key phase restarts at every entry. The table starts on an 8 byte boundary, so
// for the table the entry relative and the absolute offset agree.
static void XorRange(BYTE* data, uint64_t relOffset, size_t len)
{
    for (size_t i = 0; i < len; ++i) data[i] ^= XOR_KEY[(relOffset + i) & 7];
}

static const char* CallerModule(void* retAddr, char* buf, size_t n)
{
    HMODULE m = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)retAddr, &m);
    if (!m || !GetModuleFileNameA(m, buf, (DWORD)n)) { strncpy_s(buf, n, "?", _TRUNCATE); return buf; }
    const char* base = strrchr(buf, '\\');
    return base ? base + 1 : buf;
}

static bool EndsWithPackageName(const wchar_t* path)
{
    if (!path) return false;
    size_t n = wcslen(path);
    const wchar_t* tail = L"content.kspkg";
    size_t t = wcslen(tail);
    return n >= t && _wcsicmp(path + n - t, tail) == 0;
}

static bool IsPackageHandle(HANDLE h)
{
    EnterCriticalSection(&g_cs);
    bool found = false;
    for (HANDLE x : g_pkgHandles) if (x == h) { found = true; break; }
    LeaveCriticalSection(&g_cs);
    return found;
}

static void TrackHandle(HANDLE h, const wchar_t* path, void* caller)
{
    EnterCriticalSection(&g_cs);
    g_pkgHandles.push_back(h);
    if (g_pkgSize == 0) {
        LARGE_INTEGER sz = {};
        if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0x4000000) {
            g_pkgSize = (uint64_t)sz.QuadPart;
            g_tocSize = 0x4000000;
            g_tocStart = g_pkgSize - g_tocSize;
        }
    }
    LeaveCriticalSection(&g_cs);
    char mod[MAX_PATH];
    Log("overlay: package opened, handle %p by %s (%ls), size %llu, table at %llu", h, CallerModule(caller, mod, sizeof mod), path,
        (unsigned long long)g_pkgSize, (unsigned long long)g_tocStart);
}

// Scan the loose folder recursively and collect the files.
static void CollectFiles(const std::wstring& dir, const std::string& rel)
{
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::string name;
        for (const wchar_t* p = fd.cFileName; *p; ++p) name.push_back((char)towlower(*p));
        std::string relPath = rel.empty() ? name : rel + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { CollectFiles(dir + L"\\" + fd.cFileName, relPath); continue; }
        Override o;
        o.loosePath = dir + L"\\" + fd.cFileName;
        o.pkgPath = relPath;
        o.size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        g_files.push_back(o);
    } while (FindNextFileW(f, &fd));
    FindClose(f);
}

// Read the real table, apply the overrides, re-encode. Called on the first table read.
static void BuildToc()
{
    if (g_tocBuilt || g_tocFailed) return;
    g_tocFailed = true;    // flipped back on success
    std::wstring pkg = g_dir + L"content.kspkg";
    HANDLE h = g_origCreateFileW(pkg.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) { Log("overlay: cannot open %ls (error %lu)", pkg.c_str(), GetLastError()); return; }
    LARGE_INTEGER sz = {};
    GetFileSizeEx(h, &sz);
    g_pkgSize = (uint64_t)sz.QuadPart;
    g_tocSize = 0x4000000;
    g_tocStart = g_pkgSize - g_tocSize;
    Log("overlay: reading the table (%llu bytes at %llu) to apply %zu override(s)", (unsigned long long)g_tocSize, (unsigned long long)g_tocStart, g_files.size());
    g_toc.resize((size_t)g_tocSize);
    LARGE_INTEGER pos; pos.QuadPart = (LONGLONG)g_tocStart;
    g_origSetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
    uint64_t done = 0;
    while (done < g_tocSize) {
        DWORD got = 0;
        DWORD want = (DWORD)std::min<uint64_t>(8u << 20, g_tocSize - done);
        if (!g_origReadFile(h, g_toc.data() + done, want, &got, nullptr) || got == 0) break;
        done += got;
    }
    g_origCloseHandle(h);
    if (done != g_tocSize) { Log("overlay: short table read (%llu of %llu)", (unsigned long long)done, (unsigned long long)g_tocSize); return; }
    XorRange(g_toc.data(), 0, (size_t)g_tocSize);

    // count used slots: they are contiguous from the start, sorted by hash, empty slots are zero
    size_t slots = (size_t)(g_tocSize / SLOT), used = 0;
    for (; used < slots; ++used) {
        const BYTE* e = g_toc.data() + used * SLOT;
        if (e[0] == 0) break;
    }
    if (used == 0 || used == slots) {
        // older layout: 32 MB table
        Log("overlay: 64 MB table not recognised (used=%zu), giving up", used);
        return;
    }
    auto hashAt = [&](size_t i) { uint64_t v; memcpy(&v, g_toc.data() + i * SLOT + 0xE8, 8); return v; };

    g_virtBase = (g_pkgSize + VIRT_ALIGN - 1) & ~(VIRT_ALIGN - 1);
    uint64_t next = g_virtBase;
    int replaced = 0, inserted = 0;
    for (auto& o : g_files) {
        if (o.pkgPath.size() >= 0xE0) { Log("overlay: path too long, skipped: %s", o.pkgPath.c_str()); continue; }
        uint64_t hash = Fnv1a64Utf16(o.pkgPath);
        size_t lo = 0, hi = used;
        while (lo < hi) { size_t mid = (lo + hi) / 2; if (hashAt(mid) < hash) lo = mid + 1; else hi = mid; }
        BYTE* e = g_toc.data() + lo * SLOT;
        bool exists = lo < used && hashAt(lo) == hash;
        if (!exists) {
            if (used + 1 >= slots) { Log("overlay: table full, cannot add %s", o.pkgPath.c_str()); continue; }
            memmove(e + SLOT, e, (used - lo) * SLOT);
            memset(e, 0, SLOT);
            memcpy(e, o.pkgPath.data(), o.pkgPath.size());
            uint16_t plen = (uint16_t)o.pkgPath.size();
            memcpy(e + 0xE6, &plen, 2);
            memcpy(e + 0xE8, &hash, 8);
            ++used; ++inserted; o.inserted = true;
        } else ++replaced;
        uint16_t flags; memcpy(&flags, e + 0xE4, 2);
        if (g_cfg.overlayClearXor) flags &= (uint16_t)~0x100;
        memcpy(e + 0xE4, &flags, 2);
        o.virtOffset = next;
        memcpy(e + 0xF0, &o.size, 8);
        memcpy(e + 0xF8, &o.virtOffset, 8);
        next = (next + o.size + VIRT_ALIGN - 1) & ~(VIRT_ALIGN - 1);
        Log("overlay: %s %s (%llu bytes) -> virtual offset %llu%s", exists ? "replace" : "add", o.pkgPath.c_str(),
            (unsigned long long)o.size, (unsigned long long)o.virtOffset, g_cfg.overlayClearXor ? "" : " (xor flag kept)");
    }
    g_virtEnd = next;
    XorRange(g_toc.data(), 0, (size_t)g_tocSize);
    g_tocBuilt = true; g_tocFailed = false;
    Log("overlay: table rebuilt, %zu entries used, %d replaced, %d added, virtual range %llu..%llu", used, replaced, inserted,
        (unsigned long long)g_virtBase, (unsigned long long)g_virtEnd);
}

static Override* FindByVirtual(uint64_t off)
{
    for (auto& o : g_files) if (off >= o.virtOffset && off < o.virtOffset + o.size) return &o;
    return nullptr;
}

// Overwrite the part of a freshly read buffer that falls into the table.
static void PatchTableRead(uint64_t off, BYTE* buf, DWORD len)
{
    uint64_t end = off + len;
    if (end <= g_tocStart || off >= g_tocStart + g_tocSize) return;
    BuildToc();
    if (!g_tocBuilt) return;
    uint64_t a = std::max(off, g_tocStart), b = std::min(end, g_tocStart + g_tocSize);
    memcpy(buf + (a - off), g_toc.data() + (a - g_tocStart), (size_t)(b - a));
}

// Fill a buffer for a read at a virtual offset from the loose file behind it.
static DWORD ReadLoose(uint64_t off, BYTE* buf, DWORD len)
{
    Override* o = FindByVirtual(off);
    if (!o) return 0;
    HANDLE h = g_origCreateFileW(o->loosePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER pos; pos.QuadPart = (LONGLONG)(off - o->virtOffset);
    g_origSetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
    uint64_t left = o->size - (off - o->virtOffset);
    DWORD want = (DWORD)std::min<uint64_t>(len, left);
    DWORD got = 0;
    if (want) g_origReadFile(h, buf, want, &got, nullptr);
    g_origCloseHandle(h);
    return got;
}

static HANDLE WINAPI Hook_CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    HANDLE h = g_origCreateFileW(name, access, share, sa, disp, flags, tmpl);
    if (h != INVALID_HANDLE_VALUE && EndsWithPackageName(name)) TrackHandle(h, name, _ReturnAddress());
    return h;
}
static HANDLE WINAPI Hook_CreateFile2(LPCWSTR name, DWORD access, DWORD share, DWORD disp, LPCREATEFILE2_EXTENDED_PARAMETERS ex)
{
    HANDLE h = g_origCreateFile2(name, access, share, disp, ex);
    if (h != INVALID_HANDLE_VALUE && EndsWithPackageName(name)) TrackHandle(h, name, _ReturnAddress());
    return h;
}
static HANDLE WINAPI Hook_CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    HANDLE h = g_origCreateFileA(name, access, share, sa, disp, flags, tmpl);
    if (h != INVALID_HANDLE_VALUE && name) {
        wchar_t w[MAX_PATH * 2] = {};
        MultiByteToWideChar(CP_ACP, 0, name, -1, w, MAX_PATH * 2 - 1);
        if (EndsWithPackageName(w)) TrackHandle(h, w, _ReturnAddress());
    }
    return h;
}
static BOOL WINAPI Hook_CloseHandle(HANDLE h)
{
    if (!g_pkgHandles.empty() && IsPackageHandle(h)) {
        EnterCriticalSection(&g_cs);
        for (size_t i = 0; i < g_pkgHandles.size(); ++i) if (g_pkgHandles[i] == h) { g_pkgHandles.erase(g_pkgHandles.begin() + i); break; }
        LeaveCriticalSection(&g_cs);
        Log("overlay: package handle %p closed", h);
    }
    return g_origCloseHandle(h);
}
static BOOL WINAPI Hook_ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, LPOVERLAPPED ov)
{
    if (g_pkgHandles.empty() || !IsPackageHandle(h)) return g_origReadFile(h, buf, n, read, ov);
    uint64_t off = 0;
    if (ov) off = ((uint64_t)ov->OffsetHigh << 32) | ov->Offset;
    else { LARGE_INTEGER cur = {}, zero = {}; g_origSetFilePointerEx(h, zero, &cur, FILE_CURRENT); off = (uint64_t)cur.QuadPart; }
    bool inTable = off + n > g_tocStart && off < g_tocStart + g_tocSize;
    char mod[MAX_PATH];

    // A virtual offset lies past the end of the package, the file itself has nothing there,
    // so the read is answered from the loose file and the file position moved as if it had.
    if (g_active && g_tocBuilt && off >= g_virtBase && off < g_virtEnd) {
        DWORD got = ReadLoose(off, (BYTE*)buf, n);
        if (read) *read = got;
        if (ov) {
            ov->Internal = 0; ov->InternalHigh = got;
            HANDLE ev = (HANDLE)((uintptr_t)ov->hEvent & ~(uintptr_t)1);
            if (ev) SetEvent(ev);
        } else {
            LARGE_INTEGER np; np.QuadPart = (LONGLONG)(off + got);
            g_origSetFilePointerEx(h, np, nullptr, FILE_BEGIN);
        }
        if (g_cfg.traceFileIo && g_traceLines < 200) {
            ++g_traceLines;
            Log("overlay: ReadFile off=%llu len=%lu -> %lu bytes from loose file%s by %s", (unsigned long long)off, n, got,
                ov ? " (overlapped)" : "", CallerModule(_ReturnAddress(), mod, sizeof mod));
        }
        return TRUE;
    }

    DWORD local = 0;
    BOOL ok = g_origReadFile(h, buf, n, read ? read : &local, ov);
    DWORD got = read ? *read : local;
    if (g_cfg.traceFileIo && g_traceLines < 200 && (!inTable || off == g_tocStart)) {
        ++g_traceLines;
        Log("overlay: ReadFile off=%llu len=%lu -> %s got=%lu%s by %s%s", (unsigned long long)off, n, ok ? "ok" : "FAIL", got,
            (!ok && GetLastError() == ERROR_IO_PENDING) ? " (async pending)" : "", CallerModule(_ReturnAddress(), mod, sizeof mod),
            inTable ? " (table, further table chunks not traced)" : "");
    }
    if (ok && got && g_active && inTable) PatchTableRead(off, (BYTE*)buf, got);
    return ok;
}

void Install()
{
    InitializeCriticalSection(&g_cs);
    if (!g_cfg.overlayEnabled && !g_cfg.traceFileIo) return;
    std::wstring folder = g_dir + g_cfg.overlayFolder;
    if (g_cfg.overlayEnabled && GetFileAttributesW(folder.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CollectFiles(folder, "");
        g_active = !g_files.empty();
        Log("overlay: %zu loose file(s) under %ls", g_files.size(), folder.c_str());
    } else if (g_cfg.overlayEnabled) {
        Log("overlay: folder %ls not present, layer idle", folder.c_str());
    }
    if (!g_active && !g_cfg.traceFileIo) return;
    int a = PatchEverywhere("CreateFileW", (void*)&Hook_CreateFileW, (void**)&g_origCreateFileW);
    int b = PatchEverywhere("CreateFileA", (void*)&Hook_CreateFileA, (void**)&g_origCreateFileA);
    int c = PatchEverywhere("CreateFile2", (void*)&Hook_CreateFile2, (void**)&g_origCreateFile2);
    int d = PatchEverywhere("ReadFile", (void*)&Hook_ReadFile, (void**)&g_origReadFile);
    PatchEverywhere("SetFilePointerEx", nullptr, (void**)&g_origSetFilePointerEx);   // original address only
    int f = PatchEverywhere("CloseHandle", (void*)&Hook_CloseHandle, (void**)&g_origCloseHandle);
    Log("overlay: file hooks installed (CreateFileW %d, CreateFileA %d, CreateFile2 %d, ReadFile %d, CloseHandle %d import slots)", a, b, c, d, f);
}

} // namespace overlay

bool OverlayRedirect(const DSTORAGE_REQUEST* request, DSTORAGE_REQUEST* redirected)
{
    using namespace overlay;
    if (!g_active || !g_tocBuilt) return false;
    if (request->Options.SourceType != DSTORAGE_REQUEST_SOURCE_FILE) return false;
    uint64_t off = request->Source.File.Offset;
    if (off < g_virtBase || off >= g_virtEnd) return false;
    Override* o = FindByVirtual(off);
    if (!o) return false;
    if (!o->dsFile) {
        EnterCriticalSection(&g_cs);
        IDStorageFactory* factory = RealDStorageFactory();
        if (!o->dsFile && factory) {
            HRESULT hr = factory->OpenFile(o->loosePath.c_str(), __uuidof(IDStorageFile), (void**)&o->dsFile);
            Log("overlay: DirectStorage open %ls -> hr=0x%08X", o->loosePath.c_str(), (unsigned)hr);
        }
        LeaveCriticalSection(&g_cs);
        if (!o->dsFile) return false;
    }
    *redirected = *request;
    redirected->Source.File.Source = o->dsFile;
    redirected->Source.File.Offset = off - o->virtOffset;
    uint64_t n = ++g_redirected;
    if (n <= 20 || (n % 500) == 0)
        Log("overlay: redirected request #%llu %s +%llu size %u -> %s", (unsigned long long)n, o->pkgPath.c_str(),
            (unsigned long long)redirected->Source.File.Offset, request->Source.File.Size, DestName(request->Options.DestinationType));
    return true;
}
