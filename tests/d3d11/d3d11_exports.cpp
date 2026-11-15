// contract: d3d11.dll carries every export Windows 10's d3d11.dll has (NT 10.0 table, its ordinals) and every one of
// Wine's d3d11.spec, and each can be called safely: the create calls hand back devices or refuse bad arguments, the
// PIX markers (D3DPerformance_*, d3d11's twins of d3d9's D3DPERF_*) are no-ops, the device-layer and driver entry
// points refuse, the WinRT interop creators refuse, and the Windows 7 kernel thunks (D3DKMT*) are Wine's gdi32 ones
// where gdi32 has them and refuse with STATUS_NOT_IMPLEMENTED where it does not.
// (exports: https://winexports.info/dlls/d3d11_dll/; Wine 8.17 dlls/d3d11/d3d11.spec; argument sizes: mingw-w64's
// lib32/d3d11.def, e.g. _D3DPerformance_EndEvent@4)
//
// every export is looked up with GetProcAddress, so a name missing from src/d3d11/d3d11.def is a failure of its own,
// and on 32-bit x86 each stub is called with the argument bytes Windows' callers push and must pop all of them.
// not exported, as in Wine: D3D11CreateDeviceForD3D12 (ordinal 1, private to Windows' D3D11On12) and
// EnableFeatureLevelUpgrade, neither with a known argument list.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include "../exports.hpp"

using PFN_CoreCreateDevice = HRESULT(WINAPI *)(IDXGIFactory *, IDXGIAdapter *, UINT, const D3D_FEATURE_LEVEL *, UINT,
                                               ID3D11Device **);
using PFN_On12CreateDevice = HRESULT(WINAPI *)(IUnknown *, UINT, const D3D_FEATURE_LEVEL *, UINT, IUnknown *const *, UINT,
                                               UINT, ID3D11Device **, ID3D11DeviceContext **, D3D_FEATURE_LEVEL *);

namespace {
constexpr uint32_t kNotImplemented = 0xc0000002; // STATUS_NOT_IMPLEMENTED

// what an all-zero call answers: a value, nothing (a void function), or anything but success (a gdi32 thunk that
// refuses a zeroed descriptor), and whether gdi32 has the function
enum class Answer { Value, Void, Refused, Unchecked };
struct Export {
  const char *name;
  unsigned arg_bytes;
  Answer answer;
  uint32_t value;
  bool gdi32;
};

const Export kExports[] = {
    {"D3DKMTCloseAdapter", 4, Answer::Refused, 0, true},
    {"D3DKMTDestroyAllocation", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTDestroyContext", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTDestroyDevice", 4, Answer::Refused, 0, true},
    {"D3DKMTDestroySynchronizationObject", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTPresent", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTQueryAdapterInfo", 4, Answer::Refused, 0, true},
    {"D3DKMTSetDisplayPrivateDriverFormat", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTSignalSynchronizationObject", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTUnlock", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTWaitForSynchronizationObject", 4, Answer::Value, kNotImplemented, false},
    {"OpenAdapter10", 4, Answer::Value, uint32_t(E_NOTIMPL), false},
    {"OpenAdapter10_2", 4, Answer::Value, uint32_t(E_NOTIMPL), false},
    {"CreateDirect3D11DeviceFromDXGIDevice", 8, Answer::Value, uint32_t(E_NOTIMPL), false},
    {"CreateDirect3D11SurfaceFromDXGISurface", 8, Answer::Value, uint32_t(E_NOTIMPL), false},
    // Wine's six-argument form (Windows': ten, @40), which Wine's own d3d10core calls: typed call below
    {"D3D11CoreCreateDevice", 0, Answer::Unchecked, 0, false},
    {"D3D11CoreCreateLayeredDevice", 20, Answer::Value, uint32_t(E_NOTIMPL), false},
    {"D3D11CoreGetLayeredDeviceSize", 8, Answer::Value, 0, false},
    {"D3D11CoreRegisterLayers", 8, Answer::Value, uint32_t(S_OK), false},
    // all-zero: SDK version 0, refused before anything is made
    {"D3D11CreateDevice", 40, Answer::Value, uint32_t(E_INVALIDARG), false},
    {"D3D11CreateDeviceAndSwapChain", 48, Answer::Value, uint32_t(E_INVALIDARG), false},
    // all-zero: no D3D12 device
    {"D3D11On12CreateDevice", 40, Answer::Value, uint32_t(E_INVALIDARG), false},
    {"D3DKMTCreateAllocation", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTCreateContext", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTCreateDevice", 4, Answer::Refused, 0, true},
    {"D3DKMTCreateSynchronizationObject", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTEscape", 4, Answer::Refused, 0, true},
    {"D3DKMTGetContextSchedulingPriority", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTGetDeviceState", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTGetDisplayModeList", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTGetMultisampleMethodList", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTGetRuntimeData", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTGetSharedPrimaryHandle", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTLock", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTOpenAdapterFromHdc", 4, Answer::Refused, 0, true},
    {"D3DKMTOpenResource", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTQueryAllocationResidency", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTQueryResourceInfo", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTRender", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTSetAllocationPriority", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTSetContextSchedulingPriority", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTSetDisplayMode", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTSetGammaRamp", 4, Answer::Value, kNotImplemented, false},
    {"D3DKMTSetVidPnSourceOwner", 4, Answer::Refused, 0, true},
    {"D3DKMTWaitForVerticalBlankEvent", 4, Answer::Value, kNotImplemented, false},
    {"D3DPerformance_BeginEvent", 8, Answer::Value, 0, false},
    {"D3DPerformance_EndEvent", 4, Answer::Value, 0, false},
    {"D3DPerformance_GetStatus", 4, Answer::Value, 0, false},
    {"D3DPerformance_SetMarker", 8, Answer::Void, 0, false},
    // Wine's d3d11.spec only
    {"D3DKMTCheckVidPnExclusiveOwnership", 4, Answer::Refused, 0, true},
    {"D3DKMTOpenAdapterFromGdiDisplayName", 4, Answer::Refused, 0, true},
};
} // namespace

int
main() {
  unsigned wrong = 0, checks = 0;
  auto expect = [&](bool ok, const char *what, const char *name = "") {
    checks++;
    if (!ok && wrong++ < 32)
      printf("wrong: %s%s%s\n", name, *name ? ": " : "", what);
  };

  HMODULE d3d11 = LoadLibraryA("d3d11.dll");
  HMODULE gdi32 = LoadLibraryA("gdi32.dll");
  if (!d3d11 || !gdi32) {
    printf("failed: LoadLibrary d3d11.dll / gdi32.dll (error %lu)\n", GetLastError());
    return 1;
  }

  bool all_resolved = true;
  for (const Export &e : kExports) {
    if (!GetProcAddress(d3d11, e.name)) {
      printf("missing export: %s\n", e.name);
      all_resolved = false;
    }
  }
  expect(all_resolved, "every d3d11.dll export resolves");
  if (!all_resolved) {
    printf("failed: %u wrong of %u checks\n", wrong, checks);
    return 1;
  }

  // the Windows 10 ordinals of the create calls and the PIX markers, which a table-reading import may name
  static const struct {
    WORD ordinal;
    const char *name;
  } kOrdinals[] = {{18, "D3D11CoreCreateDevice"}, {22, "D3D11CreateDevice"}, {23, "D3D11CreateDeviceAndSwapChain"},
                   {24, "D3D11On12CreateDevice"}, {48, "D3DPerformance_BeginEvent"}, {49, "D3DPerformance_EndEvent"},
                   {50, "D3DPerformance_GetStatus"}, {51, "D3DPerformance_SetMarker"}};
  for (auto &o : kOrdinals)
    expect(GetProcAddress(d3d11, MAKEINTRESOURCEA(o.ordinal)) == GetProcAddress(d3d11, o.name), "is at its Windows ordinal",
           o.name);

  // every stub, called with all-zero arguments of the size Windows' callers push (a gdi32 thunk with a zeroed
  // descriptor, which is how it refuses)
  for (const Export &e : kExports) {
    FARPROC proc = GetProcAddress(d3d11, e.name);
    if (e.gdi32)
      expect(proc == GetProcAddress(gdi32, e.name), "is gdi32's function", e.name);
    if (e.answer == Answer::Unchecked)
      continue;
    ExportCall call = e.gdi32 ? call_export(proc, e.arg_bytes) : call_export(proc, e.arg_bytes, 0);
    expect(call.unpopped == 0, "pops exactly the argument bytes Windows' callers push", e.name);
    if (e.answer == Answer::Value)
      expect(uint32_t(call.result) == e.value, "answers what Windows / Wine answer for all-zero arguments", e.name);
    else if (e.answer == Answer::Refused)
      expect(uint32_t(call.result) != 0, "refuses an all-zero descriptor", e.name);
  }

  // the out pointers the refusing stubs clear
  {
    auto create = reinterpret_cast<HRESULT(WINAPI *)(IDXGIDevice *, IUnknown **)>(
        reinterpret_cast<void *>(GetProcAddress(d3d11, "CreateDirect3D11DeviceFromDXGIDevice")));
    IUnknown *object = reinterpret_cast<IUnknown *>(1);
    expect(create(nullptr, &object) == E_NOTIMPL && !object, "clears its out pointer",
           "CreateDirect3D11DeviceFromDXGIDevice");
  }

  // the PIX markers through a frame the way a PIX-instrumented title brackets one
  {
    auto begin = reinterpret_cast<INT(WINAPI *)(DWORD, LPCWSTR)>(
        reinterpret_cast<void *>(GetProcAddress(d3d11, "D3DPerformance_BeginEvent")));
    auto marker = reinterpret_cast<void(WINAPI *)(DWORD, LPCWSTR)>(
        reinterpret_cast<void *>(GetProcAddress(d3d11, "D3DPerformance_SetMarker")));
    auto end = reinterpret_cast<INT(WINAPI *)(void *)>(
        reinterpret_cast<void *>(GetProcAddress(d3d11, "D3DPerformance_EndEvent")));
    begin(0xffff0000, L"frame");
    marker(0xff00ff00, L"marker");
    end(nullptr);
  }

  // the create calls make devices
  {
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    D3D_FEATURE_LEVEL level{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device,
                                   &level, &context);
    expect(hr == S_OK && device && context && level >= D3D_FEATURE_LEVEL_11_0,
           "D3D11CreateDevice(HARDWARE) is S_OK with a device of 11_0 or more");
    if (context)
      context->Release();
    if (device)
      device->Release();
  }
  {
    ID3D11Device *device = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                               D3D11_SDK_VERSION, nullptr, nullptr, &device, nullptr, nullptr);
    expect(hr == S_OK && device, "D3D11CreateDeviceAndSwapChain without a swap chain is S_OK with a device");
    if (device)
      device->Release();
  }
  {
    IDXGIFactory1 *factory = nullptr;
    IDXGIAdapter *adapter = nullptr;
    ID3D11Device *device = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))) &&
        SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
      auto core = reinterpret_cast<PFN_CoreCreateDevice>(
          reinterpret_cast<void *>(GetProcAddress(d3d11, "D3D11CoreCreateDevice")));
      HRESULT hr = core(factory, adapter, 0, nullptr, 0, &device);
      expect(hr == S_OK && device, "D3D11CoreCreateDevice on DXGI's first adapter is S_OK with a device");
    } else {
      expect(false, "DXGI has a factory and an adapter");
    }
    if (device)
      device->Release();
    if (adapter)
      adapter->Release();
    if (factory)
      factory->Release();
  }
  {
    auto on12 = reinterpret_cast<PFN_On12CreateDevice>(
        reinterpret_cast<void *>(GetProcAddress(d3d11, "D3D11On12CreateDevice")));
    ID3D11Device *device = reinterpret_cast<ID3D11Device *>(1);
    HRESULT hr = on12(nullptr, 0, nullptr, 0, nullptr, 0, 0, &device, nullptr, nullptr);
    expect(hr == E_INVALIDARG && !device, "D3D11On12CreateDevice without a D3D12 device is E_INVALIDARG, no device");
  }

  FreeLibrary(gdi32);
  FreeLibrary(d3d11);

  printf("%s: %u wrong of %u checks\n", wrong ? "failed" : "passed", wrong, checks);
  return wrong ? 1 : 0;
}
