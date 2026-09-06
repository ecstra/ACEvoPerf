// Engine flags (gflags) - located by scanning the game exe for the
// FlagRegisterer call sites and written directly into FLAGS_* storage.
//
// The release build only parses a small whitelist of single-dash switches
// from the command line, so the generic gflags parser never sees anything.
// Every DEFINE_bool/int32/double/string still registers itself at startup via
//   FlagRegisterer(name, help, __FILE__, &FLAGS_x, &FLAGS_nonox)
// (rdx = name, r8 = help, r9 = file, [rsp+20h]/[rsp+28h] = storages), which is
// a stable, recognisable code pattern. We find those call sites, group them by
// constructor (one instantiation per type) and poke the storage.
#include "acevo/engine/flags.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

struct FlagInfo {
    std::string name;
    int type = -1;                  // 0 bool, 1 int32, 2 double, 3 string
    std::vector<BYTE*> storages;    // FLAGS_x and FLAGS_nonox (whichever are writable get set)
    std::string file;
};
static std::vector<FlagInfo> g_flags;
static bool g_flagsScanned = false;

struct Range { BYTE* lo = nullptr; BYTE* hi = nullptr; bool has(const void* p) const { return (BYTE*)p >= lo && (BYTE*)p < hi; } };

static bool ReadCString(const BYTE* p, const Range& r, std::string& out, size_t maxLen)
{
    out.clear();
    for (size_t i = 0; i < maxLen; ++i) {
        if (!r.has(p + i)) return false;
        BYTE c = p[i];
        if (c == 0) return !out.empty();
        if (c < 9 || c >= 127) return false;
        out.push_back((char)c);
    }
    return false;
}

static void ScanFlags()
{
    if (g_flagsScanned) return;
    g_flagsScanned = true;
    ULONGLONG t0 = GetTickCount64();
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    Range text, rdata, image;
    image.lo = base; image.hi = base + nt->OptionalHeader.SizeOfImage;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        Range r; r.lo = base + sec[i].VirtualAddress; r.hi = r.lo + sec[i].Misc.VirtualSize;
        if (memcmp(sec[i].Name, ".text", 6) == 0) text = r;
        else if (memcmp(sec[i].Name, ".rdata", 7) == 0) rdata = r;
    }
    if (!text.lo || !rdata.lo) { Log("flags: .text/.rdata not found"); return; }

    // pass 1: lea r9,[rip+x] -> "<...>.cpp" followed by a call within 90 bytes -> candidate ctor
    struct Ctor { BYTE* addr; int hits; int type; };
    std::vector<Ctor> ctors;
    std::string s;
    BYTE* p = text.lo; BYTE* end = text.hi - 8;
    for (; p < end; ++p) {
        if (p[0] != 0x4C || p[1] != 0x8D || p[2] != 0x0D) continue;
        BYTE* tgt = p + 7 + *(int32_t*)(p + 3);
        if (!rdata.has(tgt) || !ReadCString(tgt, rdata, s, 300)) continue;
        size_t n = s.size();
        if (!((n > 4 && s.compare(n - 4, 4, ".cpp") == 0) || (n > 3 && s.compare(n - 3, 3, ".cc") == 0))) continue;
        for (BYTE* q = p + 7; q < p + 97 && q < end; ++q) {
            if (*q != 0xE8) continue;
            BYTE* ct = q + 5 + *(int32_t*)(q + 1);
            if (!text.has(ct)) break;
            bool found = false;
            for (auto& c : ctors) if (c.addr == ct) { c.hits++; found = true; break; }
            if (!found) ctors.push_back({ ct, 1, -1 });
            break;
        }
    }
    std::vector<Ctor> good;
    for (auto& c : ctors) if (c.hits >= 3) good.push_back(c);
    if (good.empty()) { Log("flags: no FlagRegisterer candidates found"); return; }

    // pass 2: every call to a candidate ctor -> name (last lea rdx) + storages (lea rax; mov [rsp+20h/28h],rax)
    struct Site { BYTE* ctor; std::string name; std::vector<BYTE*> st; std::string file; };
    std::vector<Site> sites;
    for (p = text.lo; p < end; ++p) {
        if (*p != 0xE8) continue;
        BYTE* ct = p + 5 + *(int32_t*)(p + 1);
        bool isCtor = false;
        for (auto& c : good) if (c.addr == ct) { isCtor = true; break; }
        if (!isCtor) continue;
        Site site; site.ctor = ct;
        BYTE* lo = p - 220; if (lo < text.lo) lo = text.lo;
        BYTE* nameLea = nullptr; BYTE* fileLea = nullptr;
        BYTE* curStorage = nullptr; BYTE* defStorage = nullptr;   // last [rsp+20h] / [rsp+28h] before the call
        for (BYTE* q = lo; q + 7 <= p; ++q) {
            if (q[1] != 0x8D) continue;
            if (q[0] == 0x48 && q[2] == 0x15) nameLea = q;                    // lea rdx
            else if (q[0] == 0x4C && q[2] == 0x0D) fileLea = q;               // lea r9
            else if (q[0] == 0x48 && q[2] == 0x05) {                          // lea rax
                BYTE* tgt = q + 7 + *(int32_t*)(q + 3);
                // must be followed (within 8 bytes) by mov [rsp+20h|28h],rax
                for (BYTE* m = q + 7; m < q + 15 && m + 5 <= p; ++m)
                    if (m[0] == 0x48 && m[1] == 0x89 && m[2] == 0x44 && m[3] == 0x24 && (m[4] == 0x20 || m[4] == 0x28)) {
                        if (image.has(tgt)) { if (m[4] == 0x20) curStorage = tgt; else defStorage = tgt; }
                        break;
                    }
            }
        }
        if (curStorage) site.st.push_back(curStorage);
        if (defStorage && defStorage != curStorage) site.st.push_back(defStorage);
        if (!nameLea) continue;
        BYTE* nt2 = nameLea + 7 + *(int32_t*)(nameLea + 3);
        if (!rdata.has(nt2) || !ReadCString(nt2, rdata, site.name, 120)) continue;
        bool ident = true;
        for (char c : site.name) if (!(isalnum((unsigned char)c) || c == '_')) { ident = false; break; }
        if (!ident || site.st.empty()) continue;
        if (fileLea) { BYTE* ft = fileLea + 7 + *(int32_t*)(fileLea + 3); if (rdata.has(ft)) ReadCString(ft, rdata, site.file, 300); }
        sites.push_back(site);
    }

    // type per ctor from well-known flags
    struct Known { const char* n; int t; } known[] = { {"no_intro",0}, {"vr",0}, {"dumplevel",1}, {"opponent_count",1}, {"ipd_mm",2}, {"track_limit_safezone",2}, {"log_file",3}, {"startup_scene",3} };
    for (auto& c : good)
        for (auto& si : sites) if (si.ctor == c.addr)
            for (auto& k : known) if (si.name == k.n) c.type = k.t;
    for (auto& si : sites) {
        FlagInfo fi; fi.name = si.name; fi.storages = si.st;
        for (auto& c : good) if (c.addr == si.ctor) fi.type = c.type;
        size_t sl = si.file.find_last_of('\\');
        fi.file = (sl == std::string::npos) ? si.file : si.file.substr(sl + 1);
        bool dup = false;
        for (auto& f : g_flags) if (f.name == fi.name) { dup = true; break; }
        if (!dup) g_flags.push_back(fi);
    }
    int typed = 0; for (auto& f : g_flags) if (f.type >= 0) typed++;
    Log("flags: scanned exe in %llu ms: %zu ctor candidates, %zu flags (%d typed)", GetTickCount64() - t0, good.size(), g_flags.size(), typed);
}

static bool Writable(const void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return false;
    DWORD pr = mbi.Protect & 0xFF;
    return mbi.State == MEM_COMMIT && (pr == PAGE_READWRITE || pr == PAGE_WRITECOPY || pr == PAGE_EXECUTE_READWRITE || pr == PAGE_EXECUTE_WRITECOPY);
}

static void SplitFlag(const std::wstring& f, std::string& name, std::string& val)
{
    size_t eq = f.find(L'=');
    std::wstring wname = f.substr(0, eq);
    std::wstring wval = (eq == std::wstring::npos) ? L"true" : f.substr(eq + 1);
    name.assign(wname.begin(), wname.end());
    val.assign(wval.begin(), wval.end());
}

static void WriteFlag(const std::string& name, const std::string& val, const char* phase)
{
    FlagInfo* fi = nullptr;
    for (auto& x : g_flags) if (x.name == name) { fi = &x; break; }
    if (!fi) { Log("flag %s: not found in this game build (ignored)", name.c_str()); return; }
    if (fi->type == 3) { Log("flag %s: string flags are not supported (ignored)", name.c_str()); return; }
    if (fi->type < 0) { Log("flag %s: unknown type (ignored)", name.c_str()); return; }
    int written = 0;
    for (BYTE* st : fi->storages) {
        if (!Writable(st)) continue;
        if (fi->type == 0) {
            std::string v = val; for (auto& ch : v) ch = (char)tolower((unsigned char)ch);
            bool b = (v == "1" || v == "true" || v == "yes" || v == "on" || v == "t");
            bool old = *(bool*)st; *(bool*)st = b;
            Log("flag %s = %s (bool, was %s) @%p [%s, %s]", name.c_str(), b ? "true" : "false", old ? "true" : "false", st, fi->file.c_str(), phase);
        } else if (fi->type == 1) {
            int v = atoi(val.c_str()); int old = *(int*)st; *(int*)st = v;
            Log("flag %s = %d (int32, was %d) @%p [%s, %s]", name.c_str(), v, old, st, fi->file.c_str(), phase);
        } else if (fi->type == 2) {
            double v = atof(val.c_str()); double old = *(double*)st; *(double*)st = v;
            Log("flag %s = %g (double, was %g) @%p [%s, %s]", name.c_str(), v, old, st, fi->file.c_str(), phase);
        }
        ++written;
    }
    if (!written) Log("flag %s: no writable storage found (ignored)", name.c_str());
}

void ApplyFlags(const char* phase)
{
    if (g_cfg.flags.empty()) return;
    ScanFlags();
    for (auto& f : g_cfg.flags) {
        std::string name, val;
        SplitFlag(f, name, val);
        if (val == "auto") {
            if (strcmp(phase, "early") == 0) Log("flag %s: auto, written once the game creates its DXGI factory and the card is known", name.c_str());
            continue;
        }
        WriteFlag(name, val, phase);
    }
}

// The auto values the mod picks from the card, written as soon as it is known. Only the tile
// pool has a rule, any other flag set to auto is reported and left alone.
void ApplyAutoFlags(int tilePoolMb)
{
    for (auto& f : g_cfg.flags) {
        std::string name, val;
        SplitFlag(f, name, val);
        if (val != "auto") continue;
        if (name == "tile_pool_mb") WriteFlag(name, std::to_string(tilePoolMb), "auto");
        else Log("flag %s: auto has no rule for this flag (ignored)", name.c_str());
    }
}
