// contract: what a Direct3D 10 title can ask a device and its state objects for, and what it must get back.
//
// The list is the set Wine's own d3d10 and d3d10_1 tests query (dlls/d3d10/tests, dlls/d3d10_1/tests), which is the
// evidence for what an application of this generation actually asks: ID3D10Device, and at 10_1 ID3D10Device1, on the
// device; ID3D10Multithread, whose state a title both sets and reads back; ID3D10InfoQueue, which a debug-layer title
// walks; and IDXGIDevice, which is how it reaches the adapter it was made on. The Effects runtime's own contracts are
// d3d10_effects.cpp's, not this file's.
//
// An interface this build answers E_NOINTERFACE for is not a failure of the test: it is a hole in the layer, and this
// cell is what shows which holes a title would fall into. It is a window test because the device it queries is the
// one that presents.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d10_1.h>
#include <d3d10effect.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdio>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {

unsigned wrong = 0, checks = 0;

void
expect(bool ok, const char *what) {
  checks++;
  if (!ok)
    printf("wrong: %s\n", what), wrong++;
}

// one interface the title asks for, and whether this build answers it
struct Ask {
  const char *name;
  REFIID iid;
  bool required;    // what a Direct3D 10 title needs, so E_NOINTERFACE here is a hole in the layer
};

#define ASK(Type) {#Type, __uuidof(Type), true}

ComPtr<ID3D10Device1> device;

void
ask_device(const Ask &ask) {
  ComPtr<IUnknown> found;
  const HRESULT hr = device->QueryInterface(ask.iid, &found);
  if (ask.required)
    expect(SUCCEEDED(hr) && found, (std::string("the device does not answer ") + ask.name).c_str());
  else
    printf("  %s: 0x%08lx\n", ask.name, (unsigned long)hr);
}

} // namespace

int
main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  WNDCLASSA wc{0, DefWindowProcA, 0, 0, GetModuleHandleA(nullptr), nullptr, nullptr, nullptr, nullptr, "d3d10_surface"};
  RegisterClassA(&wc);
  HWND window = CreateWindowA("d3d10_surface", "d3d10_surface", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                              wc.hInstance, nullptr);
  if (!window) {
    printf("failed: no window\n");
    return 1;
  }

  ComPtr<IDXGIFactory1> factory;
  expect(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), &factory)), "no DXGI factory");
  ComPtr<IDXGIAdapter> adapter;
  if (FAILED(factory->EnumAdapters(0, &adapter)) || !adapter) {
    printf("failed: no adapter to ask a device of\n");
    DestroyWindow(window);
    return 1;
  }

  // from d3d10_1.dll itself: the toolchain has no import library for it (the same way tests/d3d10/d3d10_device.cpp
  // reaches it, which is also how a 10.1 title does -- it loads the DLL and asks for the name)
  auto create = reinterpret_cast<decltype(&D3D10CreateDevice1)>(
      (void *)GetProcAddress(LoadLibraryA("d3d10_1.dll"), "D3D10CreateDevice1")
  );
  if (!create) {
    printf("failed: no D3D10CreateDevice1 in d3d10_1.dll\n");
    DestroyWindow(window);
    return 1;
  }
  const HRESULT made = create(adapter.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, D3D10_CREATE_DEVICE_BGRA_SUPPORT,
                              D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &device);
  if (FAILED(made) || !device) {
    printf("failed: D3D10CreateDevice1 at 10_1 gave no device (0x%08lx)\n", (unsigned long)made);
    DestroyWindow(window);
    return 1;
  }
  checks++;

  const Ask asks[] = {
      ASK(ID3D10Device),
      ASK(ID3D10Device1),
      ASK(ID3D10Multithread),
      ASK(IDXGIDevice),
  };
  for (const Ask &one : asks)
    ask_device(one);

  // ID3D10InfoQueue is answered only by a device created with the debug layer on: Wine's d3d10_1 test expects it to
  // fail on an ordinary device (dlls/d3d10_1/tests/d3d10_1.c:296-300, check_interface(..., FALSE, FALSE) with the
  // comment "Non-debug mode"). Asking here on a device that was not created debuggable, the answer must be no.
  {
    ComPtr<IUnknown> queue;
    const HRESULT hr = device->QueryInterface(__uuidof(ID3D10InfoQueue), &queue);
    expect(FAILED(hr) && !queue, "a device without the debug layer answered ID3D10InfoQueue");
  }

  // the state objects a title creates and then re-queries: a device that answers them at creation but not on the
  // object leaves the title without the interface it asked for by name
  {
    ComPtr<ID3D10BlendState1> blend;
    D3D10_BLEND_DESC1 desc{};
    desc.IndependentBlendEnable = TRUE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D10_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D10_BLEND_ZERO;
    desc.RenderTarget[0].SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D10_BLEND_ZERO;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;
    const HRESULT hr = device->CreateBlendState1(&desc, &blend);
    expect(SUCCEEDED(hr) && blend, "CreateBlendState1 gave no state");
    if (blend) {
      ComPtr<ID3D10BlendState> base;
      expect(SUCCEEDED(blend->QueryInterface(__uuidof(ID3D10BlendState), &base)) && base,
             "a 10.1 blend state does not answer its 10.0 interface");
      D3D10_BLEND_DESC1 read{};
      blend->GetDesc1(&read);
      expect(read.RenderTarget[0].SrcBlend == D3D10_BLEND_ONE &&
                 read.RenderTarget[0].RenderTargetWriteMask == D3D10_COLOR_WRITE_ENABLE_ALL,
             "the 10.1 blend state does not read back what it was made with");
    }
  }

  // the device's own numbers, which decide what a title lays out
  {
    D3D10_FEATURE_LEVEL1 level = device->GetFeatureLevel();
    expect(level == D3D10_FEATURE_LEVEL_10_1, "a device created at 10_1 reports another feature level");
    // a live device's removal reason is S_OK, and the call must not be refused: a title reads it to decide whether
    // to keep drawing (Microsoft Learn, ID3D10Device::GetDeviceRemovedReason)
    expect(device->GetDeviceRemovedReason() == S_OK, "a working device reported a removal reason");
    // SetMultithreadProtected takes the new state and returns the one before it (Microsoft Learn,
    // ID3D10Multithread::SetMultithreadProtected: "Return value: the previous value"), and Get reads what the device
    // now says. D3D10CreateDevice1 already set it: the device is protected unless it was created SINGLETHREADED
    // (d3d10core.cpp), so the state to compare against is the device's, not a constant this test assumes.
    (void)level;
    ComPtr<ID3D10Multithread> multithread;
    if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D10Multithread), &multithread)) && multithread) {
      const WINBOOL before = multithread->GetMultithreadProtected();
      const WINBOOL target = before ? FALSE : TRUE;
      expect(multithread->SetMultithreadProtected(target) == before,
             "SetMultithreadProtected did not return the state the device was in");
      expect(multithread->GetMultithreadProtected() == target, "the state the call set is not the state it reports");
      expect(multithread->SetMultithreadProtected(before) == target, "restoring the state did not return the new one");
      expect(multithread->GetMultithreadProtected() == before, "the device did not go back to its first state");
    } else {
      expect(false, "the device does not answer ID3D10Multithread for its state to be read");
    }
  }

  device.Reset();
  DestroyWindow(window);
  printf("%s: %u wrong of %u checks\n", wrong ? "failed" : "passed", wrong, checks);
  return wrong != 0;
}
