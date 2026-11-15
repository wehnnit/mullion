/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "mullion_names.hpp"
#include "com/com_object.hpp"
#include "com/com_pointer.hpp"
#include "config/config.hpp"
#include "../mullion/mullion_router.hpp"
#include "d3d11_device.hpp"
#include "d3d11_on12.hpp"
#include <bit>
#include "log/log.hpp"
#include "util_string.hpp"
#include "mullion_capture.hpp"
#include <exception>

namespace mullion {
Logger Logger::s_instance("d3d11.log");

D3D_FEATURE_LEVEL
GetMaxFeatureLevel(WMT::Device device) {
  D3D_FEATURE_LEVEL maxFeatureLevel =
      device.supportsFamily(WMTGPUFamilyApple7) ? D3D_FEATURE_LEVEL_11_1 : D3D_FEATURE_LEVEL_11_0;

  if (auto fl_override = Config::getInstance().getOption<std::string>("d3d11.maxFeatureLevel", "");
      fl_override != "") {
    static const std::unordered_map<std::string, D3D_FEATURE_LEVEL> s_feature_levels = {{
        {"12_1", D3D_FEATURE_LEVEL_12_1},
        {"12_0", D3D_FEATURE_LEVEL_12_0},
        {"11_1", D3D_FEATURE_LEVEL_11_1},
        {"11_0", D3D_FEATURE_LEVEL_11_0},
        {"10_1", D3D_FEATURE_LEVEL_10_1},
        {"10_0", D3D_FEATURE_LEVEL_10_0},
        {"9_3", D3D_FEATURE_LEVEL_9_3},
        {"9_2", D3D_FEATURE_LEVEL_9_2},
        {"9_1", D3D_FEATURE_LEVEL_9_1},
    }};
    if (auto iter = s_feature_levels.find(fl_override); iter != s_feature_levels.end()) {
      maxFeatureLevel = iter->second;
    }
  }

  return maxFeatureLevel;
}

// the first of the feature levels asked for (or D3D11's default ones) that the device has
static HRESULT
SelectFeatureLevel(
    WMT::Device device, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, D3D_FEATURE_LEVEL &devFeatureLevel
) {
  // Feature levels to probe if the
  // application does not specify any.
  std::array<D3D_FEATURE_LEVEL, 6> defaultFeatureLevels = {
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1,
  };

  if (!pFeatureLevels || !FeatureLevels) {
    pFeatureLevels = defaultFeatureLevels.data();
    FeatureLevels = defaultFeatureLevels.size();
  }

  D3D_FEATURE_LEVEL maxFeatureLevel = GetMaxFeatureLevel(device);
  D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL();
  devFeatureLevel = D3D_FEATURE_LEVEL();

  Logger::info(
      str::format("Maximum supported feature level: ", maxFeatureLevel));

  for (uint32_t flId = 0; flId < FeatureLevels; flId++) {
    minFeatureLevel = pFeatureLevels[flId];

    switch (minFeatureLevel) {
    case D3D_FEATURE_LEVEL_9_1:
    case D3D_FEATURE_LEVEL_9_2:
    case D3D_FEATURE_LEVEL_9_3:
    case D3D_FEATURE_LEVEL_10_0:
    case D3D_FEATURE_LEVEL_10_1:
    case D3D_FEATURE_LEVEL_11_0:
    case D3D_FEATURE_LEVEL_11_1:
    case D3D_FEATURE_LEVEL_12_0:
    case D3D_FEATURE_LEVEL_12_1:
    case D3D_FEATURE_LEVEL_12_2:
      break;
    default:
      return E_INVALIDARG;
    }
    if (!devFeatureLevel && minFeatureLevel <= maxFeatureLevel)
      devFeatureLevel = minFeatureLevel;
  }

  if (!devFeatureLevel) {
    Logger::err(str::format("Minimum required feature level ", minFeatureLevel,
                            " not supported"));
    return DXGI_ERROR_UNSUPPORTED;
  }

  return S_OK;
}

// the bits this build keeps in D3D11CreateDevice's Flags for itself. D3D10 is implemented over the Direct3D 11
// device (its bit has been here since DXMT), and the Direct3D 9 device makes one to draw over
// (docs/ARCHITECTURE.md section 1). A bit in the call is the only way an entry point in one DLL can learn that the
// asker is another layer of this build: the static a header-inline function keeps is one per image, so state set in
// d3d9.dll is not the state d3d11.dll reads. Both bits are stripped from what GetCreationFlags hands back.
inline constexpr UINT MULLION_D3D10_DEVICE = 0x80000000u;
inline constexpr UINT MULLION_INTERNAL_DEVICE = 0x40000000u;
// what says "this build asks, not the application"
inline constexpr UINT MULLION_INTERNAL_MASK = MULLION_D3D10_DEVICE | MULLION_INTERNAL_DEVICE;

// the highest feature level a request names, as the record says it
static uint32_t
asked_feature_level(const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels) {
  uint32_t asked = 0;
  for (UINT at = 0; at < FeatureLevels; at++)
    if (pFeatureLevels && uint32_t(pFeatureLevels[at]) > asked)
      asked = uint32_t(pFeatureLevels[at]);
  return asked;
}

// the creation itself, with no routing in it: exactly one entry point decides about one request, and a decision
// made twice would be recorded twice
static HRESULT
create_device(IDXGIAdapter *pAdapter, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
              ID3D11Device **ppDevice) {
  Com<IMTLDXGIAdapter> dxgi_adapter;

  // Try to find the corresponding Metal device for the DXGI adapter
  if (FAILED(pAdapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter)))) {
    ERR("Not a Mullion adapter");
    return E_INVALIDARG;
  }

  D3D_FEATURE_LEVEL devFeatureLevel;
  if (HRESULT hr = SelectFeatureLevel(dxgi_adapter->GetMTLDevice(), pFeatureLevels, FeatureLevels, devFeatureLevel);
      FAILED(hr))
    return hr;

  try {
    Logger::info(str::format("Using feature level ", devFeatureLevel));

    auto device = CreateD3D11Device(
        CreateMullionDevice({.device = dxgi_adapter->GetMTLDevice()}),
        dxgi_adapter.ptr(), devFeatureLevel, Flags);

    return device->QueryInterface(IID_PPV_ARGS(ppDevice));
  } catch (const MTLD3DError &e) {
    Logger::err("D3D11CoreCreateDevice: Failed to create D3D11 device");
    return E_FAIL;
  } catch (const std::bad_alloc &) {
    return E_OUTOFMEMORY;
  }
}

extern "C" HRESULT WINAPI
D3D11CoreCreateDevice(IDXGIFactory *pFactory, IDXGIAdapter *pAdapter,
                      UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels,
                      UINT FeatureLevels, ID3D11Device **ppDevice) {
  InitReturnPtr(ppDevice);

  // Routed, unless the asker is another of this build's own layers. A refusal is a decision about what the
  // application asked for: `router.d3d11 = False` must not take away the device the Direct3D 9 or Direct3D 10
  // layer needs in order to render what the title did ask for. An application cannot set the bit: a Flags value
  // carrying it comes from inside this build alone.
  if (!(Flags & MULLION_INTERNAL_MASK) &&
      !mullion::router::offered(
          mullion::router::Request{mullion::router::Api::D3D11, false, false,
                                   asked_feature_level(pFeatureLevels, FeatureLevels)}))
    return E_NOINTERFACE;

  return create_device(pAdapter, Flags, pFeatureLevels, FeatureLevels, ppDevice);
}

extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
    UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
    IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice,
    D3D_FEATURE_LEVEL *pFeatureLevel,
    ID3D11DeviceContext **ppImmediateContext) {
  InitReturnPtr(ppDevice);
  InitReturnPtr(ppSwapChain);
  InitReturnPtr(ppImmediateContext);

  if (!mullion::router::offered(mullion::router::Request{
          mullion::router::Api::D3D11, false, false, asked_feature_level(pFeatureLevels, FeatureLevels)}))
    return E_NOINTERFACE;

  if (pFeatureLevel)
    *pFeatureLevel = D3D_FEATURE_LEVEL(0);

  Com<IDXGIFactory> dxgiFactory = nullptr;
  Com<IDXGIAdapter> dxgiAdapter = pAdapter;
  Com<ID3D11Device> device = nullptr;

  HRESULT hr;

  if ((ppSwapChain && !pSwapChainDesc) || SDKVersion != D3D11_SDK_VERSION)
    return E_INVALIDARG;

  if (!pAdapter) {
    if (DriverType <= D3D_DRIVER_TYPE_UNKNOWN || DriverType > D3D_DRIVER_TYPE_WARP ||
        (DriverType == D3D_DRIVER_TYPE_SOFTWARE ? !Software : Software != nullptr))
      return E_INVALIDARG;
    if (DriverType != D3D_DRIVER_TYPE_HARDWARE)
      WARN("D3D11CreateDevice: Unsupported driver type ", DriverType);
    // We'll use the first adapter returned by a DXGI factory
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

    if (FAILED(hr)) {
      Logger::err("D3D11CreateDevice: Failed to create a DXGI factory");
      return hr;
    }

    hr = dxgiFactory->EnumAdapters(0, &dxgiAdapter);

    if (FAILED(hr)) {
      Logger::err("D3D11CreateDevice: No default adapter available");
      return hr;
    }
  } else {
    // We should be able to query the DXGI factory from the adapter
    if (FAILED(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)))) {
      Logger::err(
          "D3D11CreateDevice: Failed to query DXGI factory from DXGI adapter");
      return E_INVALIDARG;
    }

    // In theory we could ignore these, but the Microsoft docs explicitly
    // state that we need to return E_INVALIDARG in case the arguments are
    // invalid. Both the driver type and software parameter can only be
    // set if the adapter itself is unspecified.
    // See:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476082(v=vs.85).aspx
    if (DriverType != D3D_DRIVER_TYPE_UNKNOWN || Software)
      return E_INVALIDARG;
  }
  // Create the actual device. Not through D3D11CoreCreateDevice, which would ask the router the same question a
  // second time: this entry point has already decided about this request.
  hr = create_device(dxgiAdapter.ptr(), Flags, pFeatureLevels, FeatureLevels, &device);

  if (FAILED(hr))
    return hr;

  // Create the swap chain, if requested
  if (ppSwapChain) {
    DXGI_SWAP_CHAIN_DESC desc = *pSwapChainDesc;
    hr = dxgiFactory->CreateSwapChain(device.ptr(), &desc, ppSwapChain);

    if (FAILED(hr)) {
      Logger::err("D3D11CreateDevice: Failed to create swap chain");
      return hr;
    }
  }
  // Write back whatever info the application requested
  if (pFeatureLevel)
    *pFeatureLevel = device->GetFeatureLevel();

  if (ppDevice)
    *ppDevice = device.ref();

  if (ppImmediateContext)
    device->GetImmediateContext(ppImmediateContext);

  // If we were unable to write back the device and the
  // swap chain, the application has no way of working
  // with the device so we should report S_FALSE here.
  if (!ppDevice && !ppImmediateContext && !ppSwapChain)
    return S_FALSE;

  return S_OK;
}

extern "C" HRESULT WINAPI D3D11CreateDevice(
    IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
    UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
    ID3D11DeviceContext **ppImmediateContext) {
  return D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags,
                                       pFeatureLevels, FeatureLevels,
                                       SDKVersion, nullptr, nullptr, ppDevice,
                                       pFeatureLevel, ppImmediateContext);
}

extern "C" HRESULT __stdcall D3D11On12CreateDevice(
    IUnknown *pDevice, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels,
    UINT FeatureLevels, IUnknown *const *ppCommandQueues, UINT NumQueues,
    UINT NodeMask, ID3D11Device **ppDevice,
    ID3D11DeviceContext **ppImmediateContext,
    D3D_FEATURE_LEVEL *pChosenFeatureLevel) {
  InitReturnPtr(ppDevice);
  InitReturnPtr(ppImmediateContext);
  if (pChosenFeatureLevel)
    *pChosenFeatureLevel = D3D_FEATURE_LEVEL();

  if (!pDevice || !ppCommandQueues || !NumQueues || !pFeatureLevels != !FeatureLevels)
    return E_INVALIDARG;

  Com<IMTLD3D11On12Source> source;
  Com<ID3D12Device> d3d12;
  Com<ID3D12CommandQueue> queue;
  if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&source))) || FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&d3d12)))) {
    ERR("D3D11On12CreateDevice: not a Mullion D3D12 device");
    return E_INVALIDARG;
  }
  if (NodeMask && ((NodeMask & (NodeMask - 1)) || std::countr_zero(NodeMask) >= d3d12->GetNodeCount()))
    return E_INVALIDARG;
  std::vector<Com<ID3D12CommandQueue>> queues;
  for (UINT i = 0; i < NumQueues; i++) {
    Com<ID3D12CommandQueue> candidate;
    Com<ID3D12Device> owner;
    if (!ppCommandQueues[i] || FAILED(ppCommandQueues[i]->QueryInterface(IID_PPV_ARGS(&candidate))) ||
        candidate->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT ||
        FAILED(candidate->GetDevice(IID_PPV_ARGS(&owner))) || owner != d3d12 ||
        std::find(queues.begin(), queues.end(), candidate) != queues.end())
      return E_INVALIDARG;
    queues.push_back(std::move(candidate));
  }
  queue = queues.front();

  // with none asked for, the D3D12 device's feature level, as far as D3D11 has it
  D3D_FEATURE_LEVEL d3d12FeatureLevel[1];
  if (!pFeatureLevels || !FeatureLevels) {
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS query = {std::size(levels), levels};
    if (FAILED(d3d12->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &query, sizeof(query))))
      return E_INVALIDARG;
    d3d12FeatureLevel[0] = std::min(query.MaxSupportedFeatureLevel, GetMaxFeatureLevel(source->GetMTLDevice()));
    pFeatureLevels = d3d12FeatureLevel;
    FeatureLevels = 1;
  }

  D3D_FEATURE_LEVEL devFeatureLevel;
  if (HRESULT hr = SelectFeatureLevel(source->GetMTLDevice(), pFeatureLevels, FeatureLevels, devFeatureLevel);
      FAILED(hr))
    return hr;

  try {
    Logger::info(str::format("D3D11On12: using feature level ", devFeatureLevel));
    Com<ID3D11Device> device;
    if (HRESULT hr = CreateD3D11On12Device(source.ptr(), d3d12.ptr(), queue.ptr(), devFeatureLevel, Flags, &device);
        FAILED(hr))
      return hr;
    if (pChosenFeatureLevel)
      *pChosenFeatureLevel = devFeatureLevel;
    if (ppImmediateContext)
      device->GetImmediateContext(ppImmediateContext);
    if (ppDevice)
      *ppDevice = device.ref();
    return ppDevice || ppImmediateContext ? S_OK : S_FALSE;
  } catch (const MTLD3DError &e) {
    Logger::err("D3D11On12CreateDevice: Failed to create D3D11 device");
    return E_FAIL;
  } catch (const std::bad_alloc &) {
    return E_OUTOFMEMORY;
  }
}

} // namespace mullion

#ifdef _WIN32

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {  
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  DisableThreadLibraryCalls(instance);
  return TRUE;
}

#endif

// PIX event markers that Windows' d3d11.dll exports alongside d3d9's D3DPERF_* (Wine's d3d11.spec has none): no-ops
// that resolve for titles that import them from d3d11. EndEvent and GetStatus take one 4-byte argument (d3d11.lib's
// _D3DPerformance_EndEvent@4, _D3DPerformance_GetStatus@4), which a 32-bit stdcall callee has to pop
extern "C" INT WINAPI D3DPerformance_BeginEvent(DWORD, LPCWSTR) { return 0; }
extern "C" INT WINAPI D3DPerformance_EndEvent(void *) { return 0; }
extern "C" DWORD WINAPI D3DPerformance_GetStatus(void *) { return 0; }
extern "C" void WINAPI D3DPerformance_SetMarker(DWORD, LPCWSTR) {}

// the device-layer entry points of Windows' table (Wine: D3D11CoreRegisterLayers, and stubs for the other two).
// there are no layers to register, and a layered device is not made
extern "C" HRESULT WINAPI D3D11CoreRegisterLayers(const void *, DWORD) { return S_OK; }
extern "C" SIZE_T WINAPI D3D11CoreGetLayeredDeviceSize(const void *, DWORD) { return 0; }
extern "C" HRESULT WINAPI D3D11CoreCreateLayeredDevice(const void *, DWORD, const void *, REFIID, void **device) {
  if (device)
    *device = nullptr;
  return E_NOTIMPL;
}

// a user-mode display driver's entry points (D3D10DDIARG_OPENADAPTER *): this d3d11.dll is not a driver
extern "C" HRESULT WINAPI OpenAdapter10(void *) { return E_NOTIMPL; }
extern "C" HRESULT WINAPI OpenAdapter10_2(void *) { return E_NOTIMPL; }

// windows.graphics.directx.direct3d11.interop.h: the WinRT IDirect3DDevice / IDirect3DSurface wrappers, which need
// Windows Runtime classes this build does not have
extern "C" HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice *, IUnknown **device) {
  if (device)
    *device = nullptr;
  return E_NOTIMPL;
}
extern "C" HRESULT WINAPI CreateDirect3D11SurfaceFromDXGISurface(IDXGISurface *, IUnknown **surface) {
  if (surface)
    *surface = nullptr;
  return E_NOTIMPL;
}

// the kernel-thunk names d3d11.dll still carries from Windows 7, each taking one D3DKMT_* argument. the ones Wine's
// gdi32 has are forwarded there (d3d11.def); the rest answer STATUS_NOT_IMPLEMENTED, as there is no kernel driver
// internal names: util_d3dkmt.h declares some of these dllimport with a typed argument, which a
// definition of the same name cannot satisfy. the export name is the Windows one (d3d11.def).
#define D3DKMT_STUB(name) \
  extern "C" LONG WINAPI mullion_D3DKMT##name(void *) { return (LONG)0xc0000002; /* STATUS_NOT_IMPLEMENTED */ }
D3DKMT_STUB(CreateAllocation)
D3DKMT_STUB(CreateContext)
D3DKMT_STUB(CreateSynchronizationObject)
D3DKMT_STUB(DestroyAllocation)
D3DKMT_STUB(DestroyContext)
D3DKMT_STUB(DestroySynchronizationObject)
D3DKMT_STUB(GetContextSchedulingPriority)
D3DKMT_STUB(GetDeviceState)
D3DKMT_STUB(GetDisplayModeList)
D3DKMT_STUB(GetMultisampleMethodList)
D3DKMT_STUB(GetRuntimeData)
D3DKMT_STUB(GetSharedPrimaryHandle)
D3DKMT_STUB(Lock)
D3DKMT_STUB(OpenResource)
D3DKMT_STUB(Present)
D3DKMT_STUB(QueryAllocationResidency)
D3DKMT_STUB(QueryResourceInfo)
D3DKMT_STUB(Render)
D3DKMT_STUB(SetAllocationPriority)
D3DKMT_STUB(SetContextSchedulingPriority)
D3DKMT_STUB(SetDisplayMode)
D3DKMT_STUB(SetDisplayPrivateDriverFormat)
D3DKMT_STUB(SetGammaRamp)
D3DKMT_STUB(SignalSynchronizationObject)
D3DKMT_STUB(Unlock)
D3DKMT_STUB(WaitForSynchronizationObject)
D3DKMT_STUB(WaitForVerticalBlankEvent)
#undef D3DKMT_STUB


extern "C" void _massert(const char *_Message, const char *_File,
                         unsigned _Line) {
  mullion::Logger::err(mullion::str::format("Assertion failed: ", _Message,
                                      "\nfile: ", _File, ":", _Line));
  std::terminate();
}

extern "C" void __cxa_pure_virtual() {
  mullion::Logger::err(mullion::str::format("Pure virtual function called"));
#ifndef NDEBUG
  __builtin_trap();
#endif
}