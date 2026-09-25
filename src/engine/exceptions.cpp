#include "acevo/engine/exceptions.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"

// The exe's import of _CxxThrowException is patched, so every throw the exe's own code makes passes
// through here first. The standard library's helpers in msvcp140.dll throw through that DLL's own import
// and are not counted, and a bare throw; is counted at its site with no type. The throw info the
// compiler emits names the type (as a mangled name) and, for std::exception types, the object carries
// a message worth logging.
typedef void (__stdcall *PFN_CxxThrowException)(void*, void*);
static PFN_CxxThrowException g_origThrow = nullptr;
static uintptr_t g_exeBase = 0;

struct ThrowSite { uintptr_t returnRva; uint32_t count; bool asked; char type[96]; char message[128]; };
static const int kMaxSites = 256;
static ThrowSite g_sites[kMaxSites];
static int g_siteCount = 0;
static std::atomic<uint32_t> g_throwsTotal{0};
static uint32_t g_throwsLastLog = 0;
static int g_ticks = 0;
static CRITICAL_SECTION g_cs;

// x64 throw info: every pointer is an RVA from the throwing module's base
struct ThrowInfoRva { unsigned attributes; int unwind; int forwardCompat; int catchableTypeArray; };
struct CatchableTypeArrayRva { int count; int types[1]; };
struct Pmd { int mdisp; int pdisp; int vdisp; };   // where a base sits in the thrown object
struct CatchableTypeRva { unsigned properties; int typeDescriptor; Pmd thisDisplacement; };
struct TypeDescriptorRaw { void* vtable; void* spare; char name[1]; };

static const char* TypeName(void* throwInfo)
{
    __try {
        auto info = (const ThrowInfoRva*)throwInfo;
        if (!info || !info->catchableTypeArray) return "?";
        auto types = (const CatchableTypeArrayRva*)(g_exeBase + info->catchableTypeArray);
        if (types->count < 1) return "?";
        auto first = (const CatchableTypeRva*)(g_exeBase + types->types[0]);
        auto descriptor = (const TypeDescriptorRaw*)(g_exeBase + first->typeDescriptor);
        return descriptor->name;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return "?";
    }
}

// The thrown object's std::exception part, or null when it cannot be caught as one. The throw info lists
// every type the object can be caught as, each with where that part sits, which is not the object's start
// when std::exception is not its first base or is a virtual one, and this is the arithmetic of vcruntime's
// __AdjustPointer. A name with "exception" or "error" in it said nothing of the kind, and reading the
// vtable at the object's start called whatever sat in its second slot, a rethrow in one Boost type.
static void* StdExceptionPart(void* object, void* throwInfo)
{
    __try {
        auto info = (const ThrowInfoRva*)throwInfo;
        if (!object || !info || !info->catchableTypeArray) return nullptr;
        auto types = (const CatchableTypeArrayRva*)(g_exeBase + info->catchableTypeArray);
        for (int i = 0; i < types->count; ++i) {
            auto type = (const CatchableTypeRva*)(g_exeBase + types->types[i]);
            auto descriptor = (const TypeDescriptorRaw*)(g_exeBase + type->typeDescriptor);
            if (strcmp(descriptor->name, ".?AVexception@std@@") != 0) continue;
            const Pmd& at = type->thisDisplacement;
            char* part = (char*)object + at.mdisp;
            if (at.pdisp >= 0) {
                const char* vbtable = *(const char* const*)((const char*)object + at.pdisp);
                part += *(const int32_t*)(vbtable + at.vdisp) + at.pdisp;
            }
            return part;
        }
        return nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// std::exception keeps what() in the second slot of its own vtable. The text is copied inside the guard,
// so a pointer what() hands back that cannot be read faults here and not in the caller.
static void Message(void* object, void* throwInfo, char* out, size_t size)
{
    out[0] = 0;
    void* part = StdExceptionPart(object, throwInfo);
    if (!part) return;
    __try {
        void** vtable = *(void***)part;
        typedef const char* (__thiscall *PFN_What)(void*);
        const char* text = ((PFN_What)vtable[1])(part);
        if (text) strncpy_s(out, size, text, _TRUNCATE);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out[0] = 0;
    }
}

static void __stdcall Hook_CxxThrowException(void* object, void* throwInfo)
{
    uintptr_t returnRva = (uintptr_t)_ReturnAddress() - g_exeBase;
    g_throwsTotal++;
    bool ask = false;
    EnterCriticalSection(&g_cs);
    ThrowSite* site = nullptr;
    for (int i = 0; i < g_siteCount; ++i) if (g_sites[i].returnRva == returnRva) { site = &g_sites[i]; break; }
    if (!site && g_siteCount < kMaxSites) {
        site = &g_sites[g_siteCount++];
        site->returnRva = returnRva;
        site->count = 0;
        site->asked = false;
        strncpy_s(site->type, TypeName(throwInfo), _TRUNCATE);
        site->message[0] = 0;
    }
    if (site) {
        site->count++;
        // Asked once per site whatever comes back, so a what() that faults, or throws back through here,
        // is not asked again on every later throw from that site.
        ask = !site->asked;
        site->asked = true;
    }
    LeaveCriticalSection(&g_cs);

    // Outside the lock, since what() is the game's own code and may take the game's own locks, which
    // another thread could hold while it waits here to count a throw of its own.
    if (ask) {
        char message[sizeof site->message];
        Message(object, throwInfo, message, sizeof message);
        EnterCriticalSection(&g_cs);
        strncpy_s(site->message, message, _TRUNCATE);
        LeaveCriticalSection(&g_cs);
    }
    g_origThrow(object, throwInfo);
}

void InstallThrowLog()
{
    if (!g_cfg.throwLog) return;
    HMODULE vcruntime = GetModuleHandleW(L"VCRUNTIME140.dll");
    if (!vcruntime) { Log("throw log: VCRUNTIME140.dll not loaded, skipped"); return; }
    void* real = (void*)GetProcAddress(vcruntime, "_CxxThrowException");
    if (!real) { Log("throw log: _CxxThrowException not found, skipped"); return; }
    InitializeCriticalSection(&g_cs);
    g_origThrow = (PFN_CxxThrowException)real;
    g_exeBase = (uintptr_t)GetModuleHandleW(nullptr);
    int patched = PatchIatByAddress(GetModuleHandleW(nullptr), real, (void*)&Hook_CxxThrowException);
    Log("throw log: %d import slot(s) of the exe patched", patched);
}

void ThrowLogTick()
{
    if (!g_origThrow || ++g_ticks % 10) return;
    uint32_t total = g_throwsTotal.load();
    uint32_t inWindow = total - g_throwsLastLog;
    g_throwsLastLog = total;
    if (!inWindow) return;
    EnterCriticalSection(&g_cs);
    ThrowSite snapshot[kMaxSites];
    int n = g_siteCount;
    memcpy(snapshot, g_sites, sizeof(ThrowSite) * n);
    for (int i = 0; i < n; ++i) g_sites[i].count = 0;
    LeaveCriticalSection(&g_cs);
    std::sort(snapshot, snapshot + n, [](const ThrowSite& a, const ThrowSite& b) { return a.count > b.count; });
    Log("[throw] %u exceptions in the last 10 s (%u since start), busiest throw sites:", inWindow, total);
    for (int i = 0; i < n && i < 8 && snapshot[i].count; ++i)
        Log("[throw]   rva 0x%06llX  x%-6u %s  %s", (unsigned long long)snapshot[i].returnRva, snapshot[i].count, snapshot[i].type, snapshot[i].message);
}
