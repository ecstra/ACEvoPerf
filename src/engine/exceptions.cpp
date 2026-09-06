#include "acevo/engine/exceptions.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"

// The exe's import of _CxxThrowException is patched, so every throw from the game's own code
// passes through here first. The throw info the compiler emits names the type (as a mangled
// name) and, for std::exception types, the object carries a message worth logging.
typedef void (__stdcall *PFN_CxxThrowException)(void*, void*);
static PFN_CxxThrowException g_origThrow = nullptr;
static uintptr_t g_exeBase = 0;

struct ThrowSite { uintptr_t returnRva; uint32_t count; char type[96]; char message[128]; };
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
struct CatchableTypeRva { unsigned properties; int typeDescriptor; };
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

// std::exception and its children keep what() in the second vtable slot
static const char* Message(void* object, const char* typeName)
{
    __try {
        if (!object || !strstr(typeName, "exception") && !strstr(typeName, "error")) return "";
        void** vtable = *(void***)object;
        typedef const char* (__thiscall *PFN_What)(void*);
        const char* text = ((PFN_What)vtable[1])(object);
        return text ? text : "";
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return "";
    }
}

static void __stdcall Hook_CxxThrowException(void* object, void* throwInfo)
{
    uintptr_t returnRva = (uintptr_t)_ReturnAddress() - g_exeBase;
    g_throwsTotal++;
    EnterCriticalSection(&g_cs);
    ThrowSite* site = nullptr;
    for (int i = 0; i < g_siteCount; ++i) if (g_sites[i].returnRva == returnRva) { site = &g_sites[i]; break; }
    if (!site && g_siteCount < kMaxSites) {
        site = &g_sites[g_siteCount++];
        site->returnRva = returnRva;
        site->count = 0;
        strncpy_s(site->type, TypeName(throwInfo), _TRUNCATE);
        site->message[0] = 0;
    }
    if (site) {
        site->count++;
        if (!site->message[0]) strncpy_s(site->message, Message(object, site->type), _TRUNCATE);
    }
    LeaveCriticalSection(&g_cs);
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
