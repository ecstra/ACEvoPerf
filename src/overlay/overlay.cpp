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
#include <mutex>

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
    bool         dsOpenFailed = false;  // not retried, see OverlayRedirect
};

static std::vector<Override> g_files;
static std::vector<BYTE> g_toc;          // modified table, XOR encoded like the original
static uint64_t g_pkgSize = 0, g_tocStart = 0, g_tocSize = 0, g_virtBase = 0, g_virtEnd = 0;
static bool g_active = false;
// The table is built by whichever thread reads it first, and any other reader waits for that in
// PatchTableRead. The flag is stored last, so a thread that sees it true also sees the finished
// table, the virtual range and every override's offset. The hooks and the redirect read it with no
// lock.
static std::once_flag g_tocOnce;
static std::atomic<bool> g_tocBuilt{false};
// A reference of our own on the real DirectStorage factory, taken the first time a redirect needs
// one and held for the process, like the IDStorageFile handles beside it. See OverlayRedirect.
static IDStorageFactory* g_dsFactory = nullptr;
static CRITICAL_SECTION g_cs;
// Whether the handle was opened with FILE_FLAG_OVERLAPPED, which Hook_ReadFile needs to tell a read
// that can complete behind its back from one that passes an OVERLAPPED only for its offset.
struct PackageHandle { HANDLE handle; bool overlapped; };
static std::vector<PackageHandle> g_pkgHandles;
// The count, so the two file hooks can skip the whole thing without touching the vector. They ran
// `g_pkgHandles.empty()` outside the lock, which reads the vector's own pointers while another
// thread's push_back or erase is rewriting them. That window is start up, when the package is
// opened while other threads are already reading files.
static std::atomic<size_t> g_pkgHandleCount{0};
static std::atomic<uint64_t> g_redirected{0};
static std::atomic<int> g_traceLines{0};   // any thread reading the package can take a line

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

static bool IsPackageHandle(HANDLE h, bool* overlapped = nullptr)
{
    EnterCriticalSection(&g_cs);
    bool found = false;
    for (const PackageHandle& x : g_pkgHandles) {
        if (x.handle != h) continue;
        found = true;
        if (overlapped) *overlapped = x.overlapped;
        break;
    }
    LeaveCriticalSection(&g_cs);
    return found;
}

static void TrackHandle(HANDLE h, const wchar_t* path, bool overlapped, void* caller)
{
    EnterCriticalSection(&g_cs);
    g_pkgHandles.push_back({ h, overlapped });
    g_pkgHandleCount.store(g_pkgHandles.size());
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
    Log("overlay: package opened, handle %p by %s (%ls), size %llu, table at %llu", h, CallerModule(caller, mod, sizeof mod), PublicPath(path).c_str(),
        (unsigned long long)g_pkgSize, (unsigned long long)g_tocStart);
}

// Scan the loose folder recursively and collect the files.
// Depth is bounded because a directory junction that points at one of its own ancestors makes this
// recurse until the stack ends, and it runs from DllMain. Nothing legitimate nests this far: the
// deepest path in the game's own package is well under it.
static void CollectFiles(const std::wstring& dir, const std::string& rel, int depth = 0)
{
    if (depth > 16) {
        Log("overlay: %ls is nested deeper than 16 folders, not descending further", PublicPath(dir).c_str());
        return;
    }
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::string name;
        for (const wchar_t* p = fd.cFileName; *p; ++p) name.push_back((char)towlower(*p));
        std::string relPath = rel.empty() ? name : rel + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { CollectFiles(dir + L"\\" + fd.cFileName, relPath, depth + 1); continue; }
        Override o;
        o.loosePath = dir + L"\\" + fd.cFileName;
        o.pkgPath = relPath;
        o.size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        g_files.push_back(o);
    } while (FindNextFileW(f, &fd));
    FindClose(f);
}

// ---------------------------------------------------------------------------
// The mod's own asset corrections, generated from the player's package
// ---------------------------------------------------------------------------
// The trackside big screens are one texture holding 64 frames in an 8 by 8 grid, stepped through
// by the material's flipbook. So the engine picks its mip from the whole sheet rather than the
// frame on show, and every coarse mip step costs eight times the detail instead of two. The asset
// ships three mip levels and the coarsest, a 512 sheet, is 64 by 64 pixels per frame on a full
// size screen. Telling the engine it has one level leaves nothing coarse to fall back to.
//
// That is a single byte, the mipLevels varint. The tiling arrays are left exactly as they are
// because the engine reads only as many of their entries as mipLevels says, measured on the
// owner's machine against both a fully rewritten header and this one. See BUG-017.
static const char* const kBigScreenTexture =
    "content\\tracks\\common_assets\\textures\\flipbooks\\led_evo_4096_64f.texture";
static const wchar_t* const kBigScreenLooseFile = L"acevo_bigscreen.texture";

// Byte index of a top level varint field's value, or -1. Walked rather than assumed at a fixed
// offset, so a header that changed shape in a game update is noticed instead of corrupted.
static int FindVarintField(const std::vector<BYTE>& data, int wanted)
{
    size_t i = 0;
    while (i < data.size()) {
        uint64_t key = 0;
        int shift = 0;
        bool ok = false;
        while (i < data.size()) { BYTE b = data[i++]; key |= (uint64_t)(b & 0x7F) << shift; shift += 7; if (!(b & 0x80)) { ok = true; break; } }
        if (!ok) return -1;

        const int field = (int)(key >> 3), wire = (int)(key & 7);
        if (wire == 0) {
            const size_t valueAt = i;
            while (i < data.size() && (data[i] & 0x80)) ++i;
            if (i >= data.size()) return -1;
            ++i;
            if (field == wanted) return (int)valueAt;
        } else if (wire == 2) {
            uint64_t len = 0;
            shift = 0;
            ok = false;
            while (i < data.size()) { BYTE b = data[i++]; len |= (uint64_t)(b & 0x7F) << shift; shift += 7; if (!(b & 0x80)) { ok = true; break; } }
            if (!ok || len > data.size() - i) return -1;
            i += (size_t)len;
        } else if (wire == 5) {
            i += 4;
        } else if (wire == 1) {
            i += 8;
        } else {
            return -1;
        }
    }
    return -1;
}

static bool ReadPackageRange(uint64_t offset, uint64_t size, std::vector<BYTE>& out)
{
    std::wstring pkg = g_dir + L"content.kspkg";
    HANDLE h = g_origCreateFileW(pkg.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    out.resize((size_t)size);
    LARGE_INTEGER pos;
    pos.QuadPart = (LONGLONG)offset;
    DWORD got = 0;
    const bool ok = g_origSetFilePointerEx(h, pos, nullptr, FILE_BEGIN)
                 && g_origReadFile(h, out.data(), (DWORD)size, &got, nullptr) && got == size;
    g_origCloseHandle(h);
    return ok;
}

// A player's own file for an entry the mod also corrects wins, since they put it there. Without
// this the mod's copy joined the list after theirs, took the slot, and theirs was never served.
static bool PlayerOverrides(const std::string& pkgPath)
{
    for (const Override& o : g_files) if (o.pkgPath == pkgPath) return true;
    return false;
}

// Every step checks what it found, because this reads the player's own package and a game update
// is free to change any of it. Anything unexpected leaves the asset alone and says so.
static void AddBigScreenFix(size_t used)
{
    if (!g_cfg.fixBigScreens) return;

    const std::string pkgPath = kBigScreenTexture;
    if (PlayerOverrides(pkgPath)) {
        Log("overlay: the mods folder has its own %s, served as it is, so the big screen fix is not applied to it", pkgPath.c_str());
        return;
    }
    const uint64_t hash = Fnv1a64Utf16(pkgPath);
    auto hashAt = [&](size_t i) { uint64_t v; memcpy(&v, g_toc.data() + i * SLOT + 0xE8, 8); return v; };
    size_t lo = 0, hi = used;
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (hashAt(mid) < hash) lo = mid + 1; else hi = mid; }
    if (lo >= used || hashAt(lo) != hash) {
        Log("overlay: the big screen flipbook is not in this package, that fix is skipped");
        return;
    }

    const BYTE* entry = g_toc.data() + lo * SLOT;
    uint16_t flags = 0;
    uint64_t size = 0, offset = 0;
    memcpy(&flags, entry + 0xE4, 2);
    memcpy(&size, entry + 0xF0, 8);
    memcpy(&offset, entry + 0xF8, 8);
    if (size < 8 || size > 4096 || offset + size > g_pkgSize) {
        Log("overlay: the big screen flipbook header is %llu bytes at %llu, not what this expects, skipped",
            (unsigned long long)size, (unsigned long long)offset);
        return;
    }

    std::vector<BYTE> header;
    if (!ReadPackageRange(offset, size, header)) { Log("overlay: cannot read the big screen flipbook header, skipped"); return; }
    if (flags & 0x100) XorRange(header.data(), 0, header.size());

    const int at = FindVarintField(header, 3);   // TextureMetadata.mipLevels
    if (at < 0 || (header[at] & 0x80)) {
        Log("overlay: the big screen flipbook header has no single byte mipLevels, skipped");
        return;
    }
    const BYTE levels = header[at];
    if (levels <= 1) {
        Log("overlay: the big screen flipbook already ships %u mip level(s), nothing to fix", levels);
        return;
    }
    header[at] = 1;

    // The file has to carry the encoding the rebuilt table will claim for it.
    if (!g_cfg.overlayClearXor) XorRange(header.data(), 0, header.size());

    const std::wstring loose = g_dir + kBigScreenLooseFile;
    HANDLE w = g_origCreateFileW(loose.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (w == INVALID_HANDLE_VALUE) { Log("overlay: cannot write %ls (error %lu), the big screen fix is skipped", PublicPath(loose).c_str(), GetLastError()); return; }
    DWORD wrote = 0;
    const bool ok = WriteFile(w, header.data(), (DWORD)header.size(), &wrote, nullptr) && wrote == header.size();
    g_origCloseHandle(w);
    if (!ok) { Log("overlay: short write to %ls, the big screen fix is skipped", PublicPath(loose).c_str()); return; }

    Override o;
    o.loosePath = loose;
    o.pkgPath = pkgPath;
    o.size = header.size();
    g_files.push_back(o);
    Log("overlay: big screens, the flipbook ships %u mip levels and is served as 1 so its 8 by 8 grid "
        "cannot fall back to a coarse mip (BUG-017)", levels);
}

// The UI engine (Cohtml 1.61) counts an element as depending on hover or focus when the part of a
// selector that holds :hover or :focus matches it, whatever the rest of the selector says. Every hover
// or focus change then restyles each counted ancestor of the element under the mouse and everything
// inside it. The game's stylesheet has parts that match nearly every page container. div:hover and
// div:focus come from the paint shop's page buttons, .component-body:hover and .component-body:focus
// from the grid editor and the paint shop's material channels. With those every div above the mouse
// counts, so each hover change restyles the whole page, 1,100 to 1,300 elements and 50 to 65 ms,
// measured with the UI probe on the settings, controls and vehicle setup pages. See BUG-014.
//
// Each part is narrowed to something the elements it styles always carry, so they still match and the
// page containers no longer count. The paint shop's page buttons always get data-page, the grid editor
// item's body always has focus-indicator, and a material channel group's body is the group's only
// child, so the group's own hover stands in for it with the same specificity.
static const char* const kUiStylesheet = "uiresources\\css\\uicomponents.css";
static const wchar_t* const kUiStylesheetLooseFile = L"acevo_uicomponents.css";

struct StyleEdit {
    const char* from;
    const char* to;
    int expected;       // occurrences in the stylesheet of 0.9.1
};

static const StyleEdit kStyleEdits[] = {
    { ".pagination > div:hover,", ".pagination > div[data-page]:hover,", 1 },
    { ".pagination > div:focus {", ".pagination > div[data-page]:focus {", 1 },
    { "ks-griditem > .component-body:hover ", "ks-griditem > .component-body.focus-indicator:hover ", 3 },
    { "ks-griditem > .component-body:focus ", "ks-griditem > .component-body.focus-indicator:focus ", 2 },
    { "ks-materialeditor #channelmode > .component-body:hover", "ks-materialeditor #channelmode:hover > .component-body", 1 },
    { "ks-materialeditor #materialenable > .component-body:hover", "ks-materialeditor #materialenable:hover > .component-body", 1 },
    { "ks-materialeditor #channels > .component-body:hover", "ks-materialeditor #channels:hover > .component-body", 1 },
};

static int CountOccurrences(const std::string& text, const char* needle)
{
    int count = 0;
    const size_t length = strlen(needle);
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + length)) ++count;
    return count;
}

static void ReplaceAll(std::string& text, const char* from, const char* to)
{
    const size_t fromLength = strlen(from), toLength = strlen(to);
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + toLength)) text.replace(at, fromLength, to);
}

// Same care as the big screen fix. A stylesheet that does not hold every part exactly as often as
// expected was changed by an update and is served untouched.
static void AddUiStyleFix(size_t used)
{
    if (!g_cfg.responsiveUi) return;

    const std::string pkgPath = kUiStylesheet;
    if (PlayerOverrides(pkgPath)) {
        Log("overlay: the mods folder has its own %s, served as it is, so the responsive UI's stylesheet fix is not applied to it", pkgPath.c_str());
        return;
    }
    const uint64_t hash = Fnv1a64Utf16(pkgPath);
    auto hashAt = [&](size_t i) { uint64_t v; memcpy(&v, g_toc.data() + i * SLOT + 0xE8, 8); return v; };
    size_t lo = 0, hi = used;
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (hashAt(mid) < hash) lo = mid + 1; else hi = mid; }
    if (lo >= used || hashAt(lo) != hash) {
        Log("overlay: the UI stylesheet is not in this package, the responsive UI serves it untouched");
        return;
    }

    const BYTE* entry = g_toc.data() + lo * SLOT;
    uint16_t flags = 0;
    uint64_t size = 0, offset = 0;
    memcpy(&flags, entry + 0xE4, 2);
    memcpy(&size, entry + 0xF0, 8);
    memcpy(&offset, entry + 0xF8, 8);
    if (size < 0x40000 || size > 0x1000000 || offset + size > g_pkgSize) {
        Log("overlay: the UI stylesheet is %llu bytes at %llu, not what this expects, served untouched",
            (unsigned long long)size, (unsigned long long)offset);
        return;
    }

    std::vector<BYTE> raw;
    if (!ReadPackageRange(offset, size, raw)) { Log("overlay: cannot read the UI stylesheet, served untouched"); return; }
    if (flags & 0x100) XorRange(raw.data(), 0, raw.size());
    std::string css(raw.begin(), raw.end());

    for (const StyleEdit& edit : kStyleEdits) {
        const int found = CountOccurrences(css, edit.from);
        if (found != edit.expected) {
            Log("overlay: the UI stylesheet has '%s' %d time(s) where %d were expected, served untouched", edit.from, found, edit.expected);
            return;
        }
    }
    for (const StyleEdit& edit : kStyleEdits) ReplaceAll(css, edit.from, edit.to);

    std::vector<BYTE> corrected(css.begin(), css.end());
    // The file has to carry the encoding the rebuilt table will claim for it.
    if (!g_cfg.overlayClearXor) XorRange(corrected.data(), 0, corrected.size());

    const std::wstring loose = g_dir + kUiStylesheetLooseFile;
    HANDLE w = g_origCreateFileW(loose.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (w == INVALID_HANDLE_VALUE) { Log("overlay: cannot write %ls (error %lu), the UI stylesheet is served untouched", PublicPath(loose).c_str(), GetLastError()); return; }
    DWORD wrote = 0;
    const bool ok = WriteFile(w, corrected.data(), (DWORD)corrected.size(), &wrote, nullptr) && wrote == corrected.size();
    g_origCloseHandle(w);
    if (!ok) { Log("overlay: short write to %ls, the UI stylesheet is served untouched", PublicPath(loose).c_str()); return; }

    Override o;
    o.loosePath = loose;
    o.pkgPath = pkgPath;
    o.size = corrected.size();
    g_files.push_back(o);
    Log("overlay: UI stylesheet, %zu hover and focus selector parts narrowed so page containers stop counting as hover dependent (BUG-014)",
        sizeof kStyleEdits / sizeof kStyleEdits[0]);
}

// A loose file that cannot be opened now is left out rather than put in the table. In the table it
// would send the game to a read that fails, and left out the game reads the package's own entry.
// The share flags are wide so the check itself locks nothing.
static void DropUnreadable()
{
    for (size_t i = 0; i < g_files.size(); ) {
        HANDLE probe = g_origCreateFileW(g_files[i].loosePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         nullptr, OPEN_EXISTING, 0, nullptr);
        if (probe != INVALID_HANDLE_VALUE) {
            g_origCloseHandle(probe);
            ++i;
            continue;
        }
        const DWORD error = GetLastError();
        Log("overlay: cannot open %ls (error %lu), left out, so the game reads the package's own %s",
            PublicPath(g_files[i].loosePath).c_str(), error, g_files[i].pkgPath.c_str());
        g_files.erase(g_files.begin() + i);
    }
}

// Read the real table, apply the overrides, re-encode. Runs once, on the first table read.
static void BuildToc()
{
    std::wstring pkg = g_dir + L"content.kspkg";
    HANDLE h = g_origCreateFileW(pkg.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) { Log("overlay: cannot open %ls (error %lu)", PublicPath(pkg).c_str(), GetLastError()); return; }

    // The sizes are the ones TrackHandle took from the handle the game opened, and they are not
    // written again here, because Hook_ReadFile reads them on other threads with no lock. A package
    // here of any other size is not the file the game is reading, so its table would be the wrong one.
    LARGE_INTEGER sz = {};
    if (!GetFileSizeEx(h, &sz) || (uint64_t)sz.QuadPart != g_pkgSize) {
        Log("overlay: %ls is %lld bytes where the game opened %llu, the table is left alone", PublicPath(pkg).c_str(),
            (long long)sz.QuadPart, (unsigned long long)g_pkgSize);
        g_origCloseHandle(h);
        return;
    }
    Log("overlay: reading the table (%llu bytes at %llu) to apply %zu override(s)", (unsigned long long)g_tocSize, (unsigned long long)g_tocStart, g_files.size());
    g_toc.resize((size_t)g_tocSize);
    LARGE_INTEGER pos; pos.QuadPart = (LONGLONG)g_tocStart;
    if (!g_origSetFilePointerEx(h, pos, nullptr, FILE_BEGIN)) {
        // Left unchecked, the reads below start at the front of the package and its first 64 MB
        // become the table.
        Log("overlay: cannot seek to the table (error %lu)", GetLastError());
        g_origCloseHandle(h);
        return;
    }
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

    // Before the mod's own corrections, so a player's file that cannot be served does not stop the
    // correction for the same entry.
    DropUnreadable();

    // The mod's own corrections join the list before it is applied, so they get a virtual offset
    // and a table slot exactly like a loose file the player put there.
    AddBigScreenFix(used);
    AddUiStyleFix(used);

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
            ++used; ++inserted;
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
    g_tocBuilt.store(true);
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
    // A second thread reading the table while the first is still building it waits here. With the
    // two plain flags this used to be, it either built the table a second time into the buffer the
    // first was filling, or gave up and handed its piece to the game unedited, leaving the game a
    // table half edited.
    std::call_once(g_tocOnce, BuildToc);
    if (!g_tocBuilt.load()) return;
    uint64_t a = std::max(off, g_tocStart), b = std::min(end, g_tocStart + g_tocSize);
    memcpy(buf + (a - off), g_toc.data() + (a - g_tocStart), (size_t)(b - a));
}

// 0.9.1 reads the table with plain synchronous reads through the C runtime. Only a read that nobody
// but its caller hears about is edited, which means a handle opened without FILE_FLAG_OVERLAPPED and
// no event in the read's OVERLAPPED, if it has one. Such a read is complete when ReadFile returns.
// Any other is left alone. On an overlapped handle, a read that goes pending gets its bytes after the
// hook has returned. One that completes at once, or any read with an event, has already set the event
// or queued its completion packet, so another thread can take the bytes, or hand the buffer to its
// next read, before an edit would land in it. Editing those safely would mean hooking the completion
// side as well, so it is said once instead.
static void NoteUneditableTableRead()
{
    static std::atomic<bool> noted{false};
    if (noted.exchange(true)) return;
    Log("overlay: the game is reading the package table on an overlapped handle or with an event, which this layer "
        "leaves unedited, so the overrides may not apply this session, and if other reads of the table were edited, an "
        "override that adds a file can leave a package entry missing or doubled");
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
    if (h != INVALID_HANDLE_VALUE && EndsWithPackageName(name)) TrackHandle(h, name, (flags & FILE_FLAG_OVERLAPPED) != 0, _ReturnAddress());
    return h;
}
static HANDLE WINAPI Hook_CreateFile2(LPCWSTR name, DWORD access, DWORD share, DWORD disp, LPCREATEFILE2_EXTENDED_PARAMETERS ex)
{
    HANDLE h = g_origCreateFile2(name, access, share, disp, ex);
    if (h != INVALID_HANDLE_VALUE && EndsWithPackageName(name))
        TrackHandle(h, name, ex && (ex->dwFileFlags & FILE_FLAG_OVERLAPPED) != 0, _ReturnAddress());
    return h;
}
static HANDLE WINAPI Hook_CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    HANDLE h = g_origCreateFileA(name, access, share, sa, disp, flags, tmpl);
    if (h != INVALID_HANDLE_VALUE && name) {
        wchar_t w[MAX_PATH * 2] = {};
        MultiByteToWideChar(CP_ACP, 0, name, -1, w, MAX_PATH * 2 - 1);
        if (EndsWithPackageName(w)) TrackHandle(h, w, (flags & FILE_FLAG_OVERLAPPED) != 0, _ReturnAddress());
    }
    return h;
}
static BOOL WINAPI Hook_CloseHandle(HANDLE h)
{
    if (g_pkgHandleCount.load() && IsPackageHandle(h)) {
        EnterCriticalSection(&g_cs);
        for (size_t i = 0; i < g_pkgHandles.size(); ++i) if (g_pkgHandles[i].handle == h) { g_pkgHandles.erase(g_pkgHandles.begin() + i); break; }
        g_pkgHandleCount.store(g_pkgHandles.size());
        LeaveCriticalSection(&g_cs);
        Log("overlay: package handle %p closed", h);
    }
    return g_origCloseHandle(h);
}
static BOOL WINAPI Hook_ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, LPOVERLAPPED ov)
{
    bool overlappedHandle = false;
    if (!g_pkgHandleCount.load() || !IsPackageHandle(h, &overlappedHandle)) return g_origReadFile(h, buf, n, read, ov);
    uint64_t off = 0;
    if (ov) off = ((uint64_t)ov->OffsetHigh << 32) | ov->Offset;
    else { LARGE_INTEGER cur = {}, zero = {}; g_origSetFilePointerEx(h, zero, &cur, FILE_CURRENT); off = (uint64_t)cur.QuadPart; }
    bool inTable = off + n > g_tocStart && off < g_tocStart + g_tocSize;
    char mod[MAX_PATH];

    // A virtual offset lies past the end of the package, the file itself has nothing there,
    // so the read is answered from the loose file and the file position moved as if it had.
    if (g_active && g_tocBuilt.load() && off >= g_virtBase && off < g_virtEnd) {
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
        if (g_cfg.traceFileIo && g_traceLines.fetch_add(1) < 200) {
            Log("overlay: ReadFile off=%llu len=%lu -> %lu bytes from loose file%s by %s", (unsigned long long)off, n, got,
                ov ? " (overlapped)" : "", CallerModule(_ReturnAddress(), mod, sizeof mod));
        }
        return TRUE;
    }

    DWORD local = 0;
    BOOL ok = g_origReadFile(h, buf, n, read ? read : &local, ov);
    // Taken now and put back at the end, because the log lines below can overwrite it, and a caller
    // whose read went pending decides what to do next from ERROR_IO_PENDING.
    const DWORD readError = ok ? ERROR_SUCCESS : GetLastError();
    DWORD got = read ? *read : local;
    if (g_cfg.traceFileIo && (!inTable || off == g_tocStart) && g_traceLines.fetch_add(1) < 200) {
        Log("overlay: ReadFile off=%llu len=%lu -> %s got=%lu%s by %s%s", (unsigned long long)off, n, ok ? "ok" : "FAIL", got,
            readError == ERROR_IO_PENDING ? " (async pending)" : "", CallerModule(_ReturnAddress(), mod, sizeof mod),
            inTable ? " (table, further table chunks not traced)" : "");
    }
    const bool othersHear = overlappedHandle || (ov && ((uintptr_t)ov->hEvent & ~(uintptr_t)1));
    if (ok && got && g_active && inTable && !othersHear) PatchTableRead(off, (BYTE*)buf, got);
    if (othersHear && g_active && inTable) NoteUneditableTableRead();
    if (!ok) SetLastError(readError);
    return ok;
}

void Install()
{
    InitializeCriticalSection(&g_cs);
    if (!g_cfg.overlayEnabled && !g_cfg.traceFileIo) return;
    std::wstring folder = g_dir + g_cfg.overlayFolder;
    if (g_cfg.overlayEnabled && GetFileAttributesW(folder.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CollectFiles(folder, "");
        Log("overlay: %zu loose file(s) under %ls", g_files.size(), PublicPath(folder).c_str());
    } else if (g_cfg.overlayEnabled) {
        Log("overlay: folder %ls not present%s", PublicPath(folder).c_str(),
            (g_cfg.fixBigScreens || g_cfg.responsiveUi) ? ", the mod's own asset fixes still apply" : ", layer idle");
    }
    // The mod's own corrections are reason enough to hook the file calls. They are generated from
    // the player's package once the table is read, so there need not be a mods folder at all.
    g_active = g_cfg.overlayEnabled && (!g_files.empty() || g_cfg.fixBigScreens || g_cfg.responsiveUi);
    if (!g_active && !g_cfg.traceFileIo) return;
    PatchEverywhere("SetFilePointerEx", nullptr, (void**)&g_origSetFilePointerEx);   // original address only
    int a = PatchEverywhere("CreateFileW", (void*)&Hook_CreateFileW, (void**)&g_origCreateFileW);
    int b = PatchEverywhere("CreateFileA", (void*)&Hook_CreateFileA, (void**)&g_origCreateFileA);
    int c = PatchEverywhere("CreateFile2", (void*)&Hook_CreateFile2, (void**)&g_origCreateFile2);
    int f = PatchEverywhere("CloseHandle", (void*)&Hook_CloseHandle, (void**)&g_origCloseHandle);

    // ReadFile goes in last, once every original its paths call is resolved, because a hook is live
    // in every module the moment its patch lands. It used to go in one statement before
    // SetFilePointerEx was resolved, so a read of the package in that window called a null pointer.
    // PatchEverywhere leaves an original unresolved when it cannot list the modules, so that is
    // checked rather than assumed.
    if (!g_origSetFilePointerEx || !g_origCreateFileW || !g_origCloseHandle) {
        Log("overlay: a file function could not be resolved, so reads of the package are not hooked and the layer is off");
        g_active = false;
        return;
    }
    int d = PatchEverywhere("ReadFile", (void*)&Hook_ReadFile, (void**)&g_origReadFile);
    Log("overlay: file hooks installed (CreateFileW %d, CreateFileA %d, CreateFile2 %d, ReadFile %d, CloseHandle %d import slots)", a, b, c, d, f);
}

bool Active() { return g_active; }

} // namespace overlay

bool OverlayRedirect(const DSTORAGE_REQUEST* request, DSTORAGE_REQUEST* redirected)
{
    using namespace overlay;
    if (!g_active || !g_tocBuilt.load()) return false;
    if (request->Options.SourceType != DSTORAGE_REQUEST_SOURCE_FILE) return false;
    uint64_t off = request->Source.File.Offset;
    if (off < g_virtBase || off >= g_virtEnd) return false;
    Override* o = FindByVirtual(off);
    if (!o) return false;

    // The file handle is read and written under the lock. The first request for a file opens it and
    // later ones only read it, and the check used to be made before the lock, on a plain pointer
    // another streaming thread could be writing, which held only because x64 keeps stores in order.
    EnterCriticalSection(&g_cs);
    if (!o->dsFile && !o->dsOpenFailed) {
        // Kept for the run once we have it. The layer cannot serve a redirect without a factory,
        // and if the game ever drops its own last reference this is what stops the proxy's real
        // factory going with it. Without that the layer would go quiet while the package table
        // still points every one of these reads past the end of the file it was rebased against.
        if (!g_dsFactory) g_dsFactory = RealDStorageFactory();
        if (g_dsFactory) {
            IDStorageFile* opened = nullptr;
            HRESULT hr = g_dsFactory->OpenFile(o->loosePath.c_str(), __uuidof(IDStorageFile), (void**)&opened);
            if (SUCCEEDED(hr) && opened) {
                o->dsFile = opened;
                Log("overlay: DirectStorage open %ls -> hr=0x%08X", PublicPath(o->loosePath).c_str(), (unsigned)hr);
            } else {
                // The file opened when the table was built, so since then it has been quarantined,
                // deleted or locked, or DirectStorage refuses it. Nothing can serve the entry now,
                // since its slot already points past the end of the package, so the read the game
                // gets is a failed one. Remembered, because retrying held this lock, which every
                // ReadFile in the process takes, and wrote a line for every request.
                o->dsOpenFailed = true;
                Log("overlay: DirectStorage cannot open %ls (hr=0x%08X), so the game's reads of %s fail this session",
                    PublicPath(o->loosePath).c_str(), (unsigned)hr, o->pkgPath.c_str());
            }
        }
    }
    IDStorageFile* file = o->dsFile;
    LeaveCriticalSection(&g_cs);
    if (!file) return false;

    *redirected = *request;
    redirected->Source.File.Source = file;
    redirected->Source.File.Offset = off - o->virtOffset;
    uint64_t n = ++g_redirected;
    if (n <= 20 || (n % 500) == 0)
        Log("overlay: redirected request #%llu %s +%llu size %u -> %s", (unsigned long long)n, o->pkgPath.c_str(),
            (unsigned long long)redirected->Source.File.Offset, request->Source.File.Size, DestName(request->Options.DestinationType));
    return true;
}
