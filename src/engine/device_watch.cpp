#include "acevo/engine/device_watch.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include <objbase.h>
#include <dbt.h>
#include <mmdeviceapi.h>

namespace {

struct KnownClass { GUID guid; const char* name; };
const KnownClass kClasses[] = {
    { { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } }, "HID" },
    { { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } }, "USB device" },
    { { 0x884B96C3, 0x56EF, 0x11D1, { 0xBC, 0x8C, 0x00, 0xA0, 0xC9, 0x14, 0x05, 0xDD } }, "keyboard" },
    { { 0x378DE44C, 0x56EF, 0x11D1, { 0xBC, 0x8C, 0x00, 0xA0, 0xC9, 0x14, 0x05, 0xDD } }, "mouse" },
    { { 0x6994AD04, 0x93EF, 0x11D0, { 0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96 } }, "audio (KS)" },
    { { 0x65E8773E, 0x8F56, 0x11D0, { 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96 } }, "audio render (KS)" },
    { { 0x65E8773D, 0x8F56, 0x11D0, { 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96 } }, "audio capture (KS)" },
    { { 0xE6327CAD, 0xDCEC, 0x4949, { 0xAE, 0x8A, 0x99, 0x1E, 0x97, 0x6A, 0x79, 0xD2 } }, "audio render endpoint" },
    { { 0x2EEF81BE, 0x33FA, 0x4800, { 0x96, 0x70, 0x1C, 0xD4, 0x74, 0x97, 0x2C, 0x3F } }, "audio capture endpoint" },
    { { 0xE6F07B5F, 0xEE97, 0x4A90, { 0xB0, 0x76, 0x33, 0xF5, 0x7B, 0xF4, 0xEA, 0xA7 } }, "monitor" },
    { { 0x5B45201D, 0xF2F2, 0x4F3B, { 0x85, 0xBB, 0x30, 0xFF, 0x1F, 0x95, 0x35, 0x99 } }, "display adapter" },
};

const char* ClassName(const GUID& guid, char* buf, size_t n)
{
    for (const auto& k : kClasses)
        if (IsEqualGUID(guid, k.guid)) return k.name;
    _snprintf_s(buf, n, _TRUNCATE, "{%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}

LRESULT CALLBACK DeviceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg != WM_DEVICECHANGE) return DefWindowProcW(hwnd, msg, wParam, lParam);
    const char* what = wParam == DBT_DEVICEARRIVAL ? "arrival" : wParam == DBT_DEVICEREMOVECOMPLETE ? "removal"
                     : wParam == DBT_DEVNODES_CHANGED ? "device nodes changed" : nullptr;
    if (!what) {
        Log("[device] WM_DEVICECHANGE 0x%llX at t=%.2fs", (unsigned long long)wParam, NowSec());
        return TRUE;
    }
    auto hdr = (const DEV_BROADCAST_HDR*)lParam;
    if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
        auto di = (const DEV_BROADCAST_DEVICEINTERFACE_W*)hdr;
        char buf[64];
        Log("[device] %s: %s %ls at t=%.2fs", what, ClassName(di->dbcc_classguid, buf, sizeof buf), di->dbcc_name, NowSec());
    } else {
        Log("[device] %s (type %lu) at t=%.2fs", what, hdr ? hdr->dbch_devicetype : 0ul, NowSec());
    }
    return TRUE;
}

struct AudioNotify : IMMNotificationClient {
    std::atomic<LONG> ref{1};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)++ref; }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = --ref; if (r == 0) delete this; return (ULONG)r; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD state) override
    {
        Log("[device] audio endpoint state -> %lu: %ls at t=%.2fs", state, id ? id : L"", NowSec());
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override { Log("[device] audio endpoint added: %ls at t=%.2fs", id ? id : L"", NowSec()); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override { Log("[device] audio endpoint removed: %ls at t=%.2fs", id ? id : L"", NowSec()); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override
    {
        Log("[device] default audio device changed (flow %d, role %d): %ls at t=%.2fs", (int)flow, (int)role, id ? id : L"(none)", NowSec());
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
};

DWORD WINAPI DeviceWatchThread(void*)
{
    SetThreadDescription(GetCurrentThread(), L"ACEvoPerf device watch");
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DeviceWndProc;
    wc.hInstance = g_self;
    wc.lpszClassName = L"ACEvoPerfDeviceWatch";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, g_self, nullptr);

    DEV_BROADCAST_DEVICEINTERFACE_W filter = {};
    filter.dbcc_size = sizeof filter;
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HDEVNOTIFY notification = hwnd ? RegisterDeviceNotificationW(hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES) : nullptr;

    IMMDeviceEnumerator* enumerator = nullptr;
    if (SUCCEEDED(com) && SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator)))
        enumerator->RegisterEndpointNotificationCallback(new AudioNotify());

    Log("[device] watch started (interface notifications %s, audio endpoint notifications %s)",
        notification ? "on" : "off", enumerator ? "on" : "off");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

} // namespace

void StartDeviceWatch()
{
    if (!g_cfg.deviceEvents) return;
    static HANDLE thread = nullptr;
    if (thread) return;
    thread = CreateThread(nullptr, 0, DeviceWatchThread, nullptr, 0, nullptr);
}
