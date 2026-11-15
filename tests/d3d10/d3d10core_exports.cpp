// contract: d3d10core.dll carries the exports of Wine's d3d10core.spec and Windows' D3D10CoreGetVersion (as DXVK does),
// and each can be called safely: D3D10CoreCreateDevice makes a device on a DXGI adapter and refuses a missing one,
// D3D10CoreGetVersion answers Windows 10's value, D3D10CoreRegisterLayers has nothing to register.
// (Wine 8.17 dlls/d3d10core/d3d10core.spec; D3D10CoreGetVersion: DXVK src/d3d10/d3d10_core.cpp)
// every export is looked up with GetProcAddress, and on 32-bit x86 each is called with the argument bytes its callers
// push and must pop all of them. not exported, as in Wine and DXVK: OpenAdapter10 / OpenAdapter10_2, a display
// driver's entry points.
#include <windows.h>
#include <d3d10.h>
#include <dxgi.h>
#include <cstdio>
#include "../exports.hpp"

using PFN_CoreCreateDevice = HRESULT(WINAPI *)(IDXGIFactory *, IDXGIAdapter *, UINT, D3D_FEATURE_LEVEL, ID3D10Device **);

int
main() {
  unsigned wrong = 0, checks = 0;
  auto expect = [&](bool ok, const char *what) {
    checks++;
    if (!ok && wrong++ < 16)
      printf("wrong: %s\n", what);
  };

  HMODULE core = LoadLibraryA("d3d10core.dll");
  if (!core) {
    printf("failed: LoadLibrary d3d10core.dll (error %lu)\n", GetLastError());
    return 1;
  }
  static const char *const kExports[] = {"D3D10CoreCreateDevice", "D3D10CoreGetVersion", "D3D10CoreRegisterLayers"};
  bool all_resolved = true;
  for (const char *name : kExports) {
    if (!GetProcAddress(core, name)) {
      printf("missing export: %s\n", name);
      all_resolved = false;
    }
  }
  expect(all_resolved, "every d3d10core.dll export resolves");
  if (!all_resolved) {
    printf("failed: %u wrong of %u checks\n", wrong, checks);
    return 1;
  }

  ExportCall version = call_export(GetProcAddress(core, "D3D10CoreGetVersion"), 0);
  expect(version.unpopped == 0, "D3D10CoreGetVersion takes no arguments");
  expect(version.result == 0xa000100041770ull, "D3D10CoreGetVersion is Windows 10's 0xa000100041770");

  ExportCall layers = call_export(GetProcAddress(core, "D3D10CoreRegisterLayers"), 0);
  expect(layers.unpopped == 0, "D3D10CoreRegisterLayers takes no arguments (Wine's d3d10core.spec)");
  expect(uint32_t(layers.result) == S_OK, "D3D10CoreRegisterLayers is S_OK");

  // all-zero: no adapter, refused before anything is made
  ExportCall refused = call_export(GetProcAddress(core, "D3D10CoreCreateDevice"), 20, 0);
  expect(refused.unpopped == 0, "D3D10CoreCreateDevice pops its five arguments");
  expect(uint32_t(refused.result) == uint32_t(E_INVALIDARG), "D3D10CoreCreateDevice without an adapter is E_INVALIDARG");

  {
    auto create = reinterpret_cast<PFN_CoreCreateDevice>(
        reinterpret_cast<void *>(GetProcAddress(core, "D3D10CoreCreateDevice")));
    IDXGIFactory *factory = nullptr;
    IDXGIAdapter *adapter = nullptr;
    ID3D10Device *device = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory))) &&
        SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
      HRESULT hr = create(factory, adapter, 0, D3D_FEATURE_LEVEL_10_0, &device);
      expect(hr == S_OK && device, "D3D10CoreCreateDevice(10_0) on DXGI's first adapter is S_OK with a device");
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

  FreeLibrary(core);
  printf("%s: %u wrong of %u checks\n", wrong ? "failed" : "passed", wrong, checks);
  return wrong ? 1 : 0;
}
