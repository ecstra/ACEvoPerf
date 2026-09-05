// INITGUID before any header so IID_IDirectInput8W is defined in this file, no dxguid.lib needed
#define INITGUID
#include "acevo/engine/input_probe.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"
#include <Xinput.h>
#include <dinput.h>

std::atomic<uint64_t> g_inputCalls{0};
std::atomic<uint64_t> g_inputUs{0};
std::atomic<uint64_t> g_inputMaxUs{0};

static LARGE_INTEGER g_inputQpf = {};
static std::atomic<int> g_slowLogBudget{20};

static void Account(const char* what, DWORD user, LONGLONG ticks, DWORD result)
{
    uint64_t us = (uint64_t)(ticks * 1000000 / g_inputQpf.QuadPart);
    g_inputCalls++;
    g_inputUs += us;
    uint64_t prev = g_inputMaxUs.load();
    while (us > prev && !g_inputMaxUs.compare_exchange_weak(prev, us)) {}
    if (us >= 1000 && g_slowLogBudget.fetch_sub(1) > 0)
        Log("[input] %s(%lu) took %llu us -> 0x%08lX", what, user, (unsigned long long)us, result);
}

// ---------------------------------------------------------------------------
// XInput
// ---------------------------------------------------------------------------
typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD, XINPUT_STATE*);
typedef DWORD (WINAPI *PFN_XInputGetCapabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
static PFN_XInputGetState g_origXInputGetState = nullptr;
static PFN_XInputGetCapabilities g_origXInputGetCapabilities = nullptr;

static DWORD WINAPI Hook_XInputGetState(DWORD user, XINPUT_STATE* state)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    DWORD r = g_origXInputGetState(user, state);
    QueryPerformanceCounter(&b);
    Account("XInputGetState", user, b.QuadPart - a.QuadPart, r);
    return r;
}

static DWORD WINAPI Hook_XInputGetCapabilities(DWORD user, DWORD flags, XINPUT_CAPABILITIES* caps)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    DWORD r = g_origXInputGetCapabilities(user, flags, caps);
    QueryPerformanceCounter(&b);
    Account("XInputGetCapabilities", user, b.QuadPart - a.QuadPart, r);
    return r;
}

// ---------------------------------------------------------------------------
// DirectInput: wrap the devices the game creates, time Poll and GetDeviceState
// ---------------------------------------------------------------------------
typedef HRESULT (WINAPI *PFN_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateDevice)(IDirectInput8W*, REFGUID, IDirectInputDevice8W**, LPUNKNOWN);
typedef HRESULT (STDMETHODCALLTYPE *PFN_Poll)(IDirectInputDevice8W*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetDeviceState)(IDirectInputDevice8W*, DWORD, LPVOID);
static PFN_DirectInput8Create g_origDirectInput8Create = nullptr;
static PFN_CreateDevice g_origCreateDevice = nullptr;
static PFN_Poll g_origPoll = nullptr;
static PFN_GetDeviceState g_origGetDeviceState = nullptr;

static HRESULT STDMETHODCALLTYPE Hook_Poll(IDirectInputDevice8W* self)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    HRESULT r = g_origPoll(self);
    QueryPerformanceCounter(&b);
    Account("IDirectInputDevice8::Poll", 0, b.QuadPart - a.QuadPart, (DWORD)r);
    return r;
}

static HRESULT STDMETHODCALLTYPE Hook_GetDeviceState(IDirectInputDevice8W* self, DWORD size, LPVOID data)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    HRESULT r = g_origGetDeviceState(self, size, data);
    QueryPerformanceCounter(&b);
    Account("IDirectInputDevice8::GetDeviceState", size, b.QuadPart - a.QuadPart, (DWORD)r);
    return r;
}

static HRESULT STDMETHODCALLTYPE Hook_CreateDevice(IDirectInput8W* self, REFGUID guid, IDirectInputDevice8W** device, LPUNKNOWN outer)
{
    HRESULT hr = g_origCreateDevice(self, guid, device, outer);
    if (SUCCEEDED(hr) && device && *device) {
        void** vt = *(void***)*device;
        HookVtableSlot(vt, 25, (void*)&Hook_Poll, (void**)&g_origPoll, "IDirectInputDevice8::Poll");
        HookVtableSlot(vt, 9, (void*)&Hook_GetDeviceState, (void**)&g_origGetDeviceState, "IDirectInputDevice8::GetDeviceState");
        Log("[input] DirectInput device created (%08lX-...)", guid.Data1);
    }
    return hr;
}

static HRESULT WINAPI Hook_DirectInput8Create(HINSTANCE inst, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    HRESULT hr = g_origDirectInput8Create(inst, version, riid, out, outer);
    if (SUCCEEDED(hr) && out && *out && riid == IID_IDirectInput8W) {
        void** vt = *(void***)*out;
        HookVtableSlot(vt, 3, (void*)&Hook_CreateDevice, (void**)&g_origCreateDevice, "IDirectInput8::CreateDevice");
    }
    Log("[input] DirectInput8Create -> hr=0x%08X", (unsigned)hr);
    return hr;
}

void InstallInputProbe()
{
    if (!g_cfg.inputProbe) return;
    QueryPerformanceFrequency(&g_inputQpf);
    HMODULE exe = GetModuleHandleW(nullptr);
    int patched = 0;

    HMODULE xinput = GetModuleHandleW(L"xinput1_4.dll");
    if (xinput) {
        g_origXInputGetState = (PFN_XInputGetState)GetProcAddress(xinput, "XInputGetState");
        g_origXInputGetCapabilities = (PFN_XInputGetCapabilities)GetProcAddress(xinput, "XInputGetCapabilities");
        if (g_origXInputGetState) patched += PatchIatByAddress(exe, (void*)g_origXInputGetState, (void*)&Hook_XInputGetState);
        if (g_origXInputGetCapabilities) patched += PatchIatByAddress(exe, (void*)g_origXInputGetCapabilities, (void*)&Hook_XInputGetCapabilities);
    }

    HMODULE dinput = GetModuleHandleW(L"dinput8.dll");
    if (dinput) {
        g_origDirectInput8Create = (PFN_DirectInput8Create)GetProcAddress(dinput, "DirectInput8Create");
        if (g_origDirectInput8Create) patched += PatchIatByAddress(exe, (void*)g_origDirectInput8Create, (void*)&Hook_DirectInput8Create);
    }

    Log("[input] probe installed: xinput1_4 %s, dinput8 %s, %d import slots patched (a DirectInput device already created before this point is not timed)",
        xinput ? "loaded" : "absent", dinput ? "loaded" : "absent", patched);
}
