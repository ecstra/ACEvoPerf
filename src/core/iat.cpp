#include "acevo/core/iat.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

int PatchIatByAddress(HMODULE mod, void* target, void* replacement)
{
    if (!mod || !target) return 0;
    int patched = 0;
    __try {
        BYTE* base = (BYTE*)mod;
        auto dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress || !dir.Size) return 0;
        auto desc = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress);
        for (; desc->Name; ++desc) {
            if (!desc->FirstThunk) continue;
            auto thunk = (IMAGE_THUNK_DATA64*)(base + desc->FirstThunk);
            for (; thunk->u1.Function; ++thunk) {
                if ((void*)thunk->u1.Function == target) {
                    DWORD old = 0;
                    if (VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) {
                        thunk->u1.Function = (ULONGLONG)replacement;
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
                        ++patched;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return patched;
    }
    return patched;
}

// kernel32's exports are stubs that jump through kernel32's own import table into
// kernelbase, so that table (and kernelbase's, and ntdll's) must never be patched
// or calling the original would recurse into the hook. kernelbase holds the real code.
int PatchEverywhere(const char* fn, void* hook, void** orig)
{
    int total = 0;
    HMODULE mods[1024]; DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &needed)) return 0;
    int count = (int)std::min<DWORD>(needed / sizeof(HMODULE), 1024);
    HMODULE skip[4] = { g_self, GetModuleHandleW(L"kernel32.dll"), GetModuleHandleW(L"kernelbase.dll"), GetModuleHandleW(L"ntdll.dll") };
    for (const wchar_t* lib : { L"kernelbase.dll", L"kernel32.dll" }) {
        HMODULE m = GetModuleHandleW(lib);
        void* target = m ? (void*)GetProcAddress(m, fn) : nullptr;
        if (!target) continue;
        if (!*orig) *orig = target;
        if (!hook) continue;      // caller only wanted the original address
        for (int i = 0; i < count; ++i) {
            bool skipThis = false;
            for (HMODULE s : skip) if (mods[i] == s) skipThis = true;
            if (!skipThis) total += PatchIatByAddress(mods[i], target, hook);
        }
    }
    return total;
}

void HookVtableSlot(void** vt, int idx, void* hook, void** orig, const char* what)
{
    if (*orig) return;
    DWORD old = 0;
    if (!VirtualProtect(&vt[idx], sizeof(void*), PAGE_READWRITE, &old)) return;
    *orig = vt[idx];
    vt[idx] = hook;
    VirtualProtect(&vt[idx], sizeof(void*), old, &old);
    Log("hooked %s", what);
}
