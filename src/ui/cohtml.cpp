#include "acevo/ui/cohtml.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"

// Layouts read from the exe's own set up code on 0.9.0 (the fields it writes before each call):
//   Library vtable  slot 1 CreateSystem(const SystemSettings&)
//   SystemSettings  0x80 bytes: +0x68 int DebuggerPort (the game passes -1), +0x6c bool EnableDebugger
//   System vtable   slot 3 CreateView(const ViewSettings&)
//   ViewSettings    +0x10 unsigned Width, +0x14 unsigned Height
// The settings structs live on the game's stack for the call only, so a patched copy is safe.
// The rest of the engine's surface that was mapped in the UI lag hunt (work execution, initial
// scripts, the view vtable) is in the doc cohtml-ui-engine and in the history before the commit
// that removed the hunt's instruments.
static const size_t kSystemSettingsSize = 0x80;
static const size_t kDebuggerPortOffset = 0x68;
static const size_t kEnableDebuggerOffset = 0x6c;
static const int kCreateSystemSlot = 1;
static const int kCreateViewSlot = 3;

typedef void* (*PFN_LibraryInitialize)(const char* licenseKey, const void* params);
typedef void* (*PFN_CreateSystem)(void* library, const void* settings);
typedef void* (*PFN_CreateView)(void* system, const void* settings);

static PFN_LibraryInitialize g_origInitialize = nullptr;
static PFN_CreateSystem g_origCreateSystem = nullptr;
static PFN_CreateView g_origCreateView = nullptr;
static HMODULE g_cohtml = nullptr;
static int g_views = 0;

static uintptr_t VtableRva(void* object)
{
    return (uintptr_t)*(void**)object - (uintptr_t)g_cohtml;
}

static void* Hook_CreateView(void* system, const void* settings)
{
    auto bytes = (const uint8_t*)settings;
    unsigned width = *(const unsigned*)(bytes + 0x10);
    unsigned height = *(const unsigned*)(bytes + 0x14);
    void* view = g_origCreateView(system, settings);
    if (!view) { Log("[ui] Cohtml view %ux%u: creation failed", width, height); return view; }
    ++g_views;
    Log("[ui] Cohtml view #%d %ux%u created, vtable at cohtml+0x%llX", g_views, width, height, (unsigned long long)VtableRva(view));
    return view;
}

static void* Hook_CreateSystem(void* library, const void* settings)
{
    uint8_t copy[kSystemSettingsSize];
    memcpy(copy, settings, sizeof copy);
    int gamePort = *(int*)(copy + kDebuggerPortOffset);
    bool gameEnabled = copy[kEnableDebuggerOffset] != 0;
    if (g_cfg.uiInspectorPort > 0) {
        *(int*)(copy + kDebuggerPortOffset) = g_cfg.uiInspectorPort;
        copy[kEnableDebuggerOffset] = 1;
    }
    Log("[ui] Cohtml system: game asks inspector port %d enabled=%d, running with port %d enabled=%d",
        gamePort, gameEnabled, *(int*)(copy + kDebuggerPortOffset), copy[kEnableDebuggerOffset]);

    void* system = g_origCreateSystem(library, copy);
    if (!system) { Log("[ui] Cohtml system creation failed"); return system; }
    Log("[ui] Cohtml system created, vtable at cohtml+0x%llX", (unsigned long long)VtableRva(system));
    HookVtableSlot(*(void***)system, kCreateViewSlot, (void*)&Hook_CreateView, (void**)&g_origCreateView, "Cohtml System::CreateView");
    return system;
}

static void* Hook_LibraryInitialize(const char* licenseKey, const void* params)
{
    void* library = g_origInitialize(licenseKey, params);
    if (!library) { Log("[ui] Cohtml library initialisation failed"); return library; }
    Log("[ui] Cohtml %s library created, vtable at cohtml+0x%llX", licenseKey ? "licensed" : "unlicensed", (unsigned long long)VtableRva(library));
    HookVtableSlot(*(void***)library, kCreateSystemSlot, (void*)&Hook_CreateSystem, (void**)&g_origCreateSystem, "Cohtml Library::CreateSystem");
    return library;
}

void InstallCohtmlHooks()
{
    g_cohtml = GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    if (!g_cohtml) { Log("[ui] cohtml.WindowsDesktop.dll not loaded at attach time, hook skipped"); return; }
    void* real = (void*)GetProcAddress(g_cohtml, "?Initialize@Library@cohtml@@SAPEAV12@PEBDAEBULibraryParams@2@@Z");
    if (!real) { Log("[ui] Cohtml Library::Initialize export not found, hook skipped"); return; }
    g_origInitialize = (PFN_LibraryInitialize)real;
    int patched = PatchIatByAddress(GetModuleHandleW(nullptr), real, (void*)&Hook_LibraryInitialize);
    Log("[ui] Cohtml Library::Initialize: %d import slot(s) of the exe patched, inspector port %d", patched, g_cfg.uiInspectorPort);
}
