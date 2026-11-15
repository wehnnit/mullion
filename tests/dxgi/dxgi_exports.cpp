// contract: dxgi.dll carries every export of Wine's dxgi.spec and the create calls of Windows' table at their Windows
// ordinals (as DXVK's dxgi.def has them), and each can be called safely: the factories are made, the debug interface
// and adapter-removal declaration answer as Windows does, and the hooks a Direct3D 10/11 runtime layers its device on
// (DXGID3D10*) refuse, as no runtime's device is made here.
// (Wine 8.17 dlls/dxgi/dxgi.spec; argument sizes: mingw-w64's lib32/dxgi.def; DXGIDeclareAdapterRemovalSupport: "Returns
// DXGI_ERROR_ALREADY_EXISTS if called more than once", Microsoft Learn)
// every export is looked up with GetProcAddress, and on 32-bit x86 each stub is called with the argument bytes its
// callers push and must pop all of them. DXGID3D10CreateDevice is Wine's seven-argument form (Windows': six, @24).
// not exported, as in Wine and DXVK: the PIX capture and compat-quirk helpers, DXGIDumpJournal,
// DXGIReportAdapterConfiguration, DXGIRevertToSxS, OpenAdapter10 and the D3DKMT thunks, none of which games import.
#include <windows.h>
#include <dxgi1_6.h>
#include <cstdio>
#include "../exports.hpp"

int
main() {
  unsigned wrong = 0, checks = 0;
  auto expect = [&](bool ok, const char *what) {
    checks++;
    if (!ok && wrong++ < 16)
      printf("wrong: %s\n", what);
  };

  HMODULE dxgi = LoadLibraryA("dxgi.dll");
  if (!dxgi) {
    printf("failed: LoadLibrary dxgi.dll (error %lu)\n", GetLastError());
    return 1;
  }
  static const char *const kExports[] = {"CreateDXGIFactory",
                                         "CreateDXGIFactory1",
                                         "CreateDXGIFactory2",
                                         "DXGIDeclareAdapterRemovalSupport",
                                         "DXGIGetDebugInterface1",
                                         "DXGID3D10CreateDevice",
                                         "DXGID3D10CreateLayeredDevice",
                                         "DXGID3D10GetLayeredDeviceSize",
                                         "DXGID3D10RegisterLayers"};
  bool all_resolved = true;
  for (const char *name : kExports) {
    if (!GetProcAddress(dxgi, name)) {
      printf("missing export: %s\n", name);
      all_resolved = false;
    }
  }
  expect(all_resolved, "every dxgi.dll export resolves");
  if (!all_resolved) {
    printf("failed: %u wrong of %u checks\n", wrong, checks);
    return 1;
  }

  static const struct {
    WORD ordinal;
    const char *name;
  } kOrdinals[] = {{9, "CreateDXGIFactory"}, {10, "CreateDXGIFactory1"}, {11, "CreateDXGIFactory2"},
                   {16, "DXGIDeclareAdapterRemovalSupport"}, {17, "DXGIGetDebugInterface1"}};
  for (auto &o : kOrdinals)
    expect(GetProcAddress(dxgi, MAKEINTRESOURCEA(o.ordinal)) == GetProcAddress(dxgi, o.name),
           "a create call is at its Windows ordinal");

  // the runtime hooks, all-zero
  static const struct {
    const char *name;
    unsigned arg_bytes;
    uint32_t value;
  } kStubs[] = {{"DXGID3D10CreateDevice", 28, uint32_t(E_NOTIMPL)},
                {"DXGID3D10CreateLayeredDevice", 20, uint32_t(E_NOTIMPL)},
                {"DXGID3D10GetLayeredDeviceSize", 8, 0},
                {"DXGID3D10RegisterLayers", 8, uint32_t(E_NOTIMPL)}};
  for (auto &s : kStubs) {
    ExportCall call = call_export(GetProcAddress(dxgi, s.name), s.arg_bytes, 0);
    expect(call.unpopped == 0, s.name);
    expect(uint32_t(call.result) == s.value, s.name);
  }

  // the factories
  {
    auto create = reinterpret_cast<HRESULT(WINAPI *)(REFIID, void **)>(
        reinterpret_cast<void *>(GetProcAddress(dxgi, "CreateDXGIFactory")));
    IDXGIFactory *factory = nullptr;
    expect(create(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory)) == S_OK && factory,
           "CreateDXGIFactory is S_OK with a factory");
    if (factory)
      factory->Release();
  }
  {
    auto create = reinterpret_cast<HRESULT(WINAPI *)(REFIID, void **)>(
        reinterpret_cast<void *>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
    IDXGIFactory1 *factory = nullptr;
    expect(create(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)) == S_OK && factory,
           "CreateDXGIFactory1 is S_OK with a factory");
    IDXGIAdapter1 *adapter = nullptr;
    expect(factory && SUCCEEDED(factory->EnumAdapters1(0, &adapter)), "the factory has an adapter");
    if (adapter)
      adapter->Release();
    if (factory)
      factory->Release();
  }
  {
    auto create = reinterpret_cast<HRESULT(WINAPI *)(UINT, REFIID, void **)>(
        reinterpret_cast<void *>(GetProcAddress(dxgi, "CreateDXGIFactory2")));
    IDXGIFactory2 *factory = nullptr;
    expect(create(0, __uuidof(IDXGIFactory2), reinterpret_cast<void **>(&factory)) == S_OK && factory,
           "CreateDXGIFactory2(0) is S_OK with a factory");
    if (factory)
      factory->Release();
  }

  // no DXGI debug layer: an interface nobody has is E_NOINTERFACE
  {
    auto debug = reinterpret_cast<HRESULT(WINAPI *)(UINT, REFIID, void **)>(
        reinterpret_cast<void *>(GetProcAddress(dxgi, "DXGIGetDebugInterface1")));
    void *object = nullptr;
    expect(debug(0, __uuidof(IDXGIFactory), &object) == E_NOINTERFACE && !object,
           "DXGIGetDebugInterface1 for a non-debug interface is E_NOINTERFACE");
  }

  // the adapter-removal declaration: once, then DXGI_ERROR_ALREADY_EXISTS
  {
    auto declare = GetProcAddress(dxgi, "DXGIDeclareAdapterRemovalSupport");
    ExportCall first = call_export(declare, 0);
    ExportCall second = call_export(declare, 0);
    expect(first.unpopped == 0 && second.unpopped == 0, "DXGIDeclareAdapterRemovalSupport takes no arguments");
    expect(uint32_t(first.result) == S_OK, "DXGIDeclareAdapterRemovalSupport is S_OK the first time");
    expect(uint32_t(second.result) == uint32_t(DXGI_ERROR_ALREADY_EXISTS),
           "DXGIDeclareAdapterRemovalSupport is DXGI_ERROR_ALREADY_EXISTS the second time");
  }

  FreeLibrary(dxgi);
  printf("%s: %u wrong of %u checks\n", wrong ? "failed" : "passed", wrong, checks);
  return wrong ? 1 : 0;
}
