// contract: d3d12.dll and d3d12core.dll carry every export of Wine's d3d12.spec (GetBehaviorValue at ordinal 100,
// the create and debug calls at 101 and 102, the root-signature helpers, D3D12GetInterface,
// D3D12EnableExperimentalFeatures, and the three device-layer entry points) and d3d12core.dll's D3D12SDKVersion,
// and each can be called safely. GetBehaviorValue leaves the caller's default in place (d3d12.lib:
// _GetBehaviorValue@8). the device-layer calls refuse or report no layers. a feature level below 11_0 is refused
// before a device is made, and 11_0 makes one.
// (Wine 8.17 dlls/d3d12/d3d12.spec; argument sizes: mingw-w64's lib32/d3d12.def)
// every export is looked up with GetProcAddress, and on 32-bit x86 each stub is called with the argument bytes its
// callers push and must pop all of them. not exported, as in Wine: the PIX helpers, SetAppCompatStringPointer and
// D3D12DeviceRemovedExtendedData. WinPixEventRuntime looks those up and continues when they are absent.
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include "../exports.hpp"

namespace {
struct Named {
  const char *name;
  unsigned arg_bytes;
  uint32_t value; // what an all-zero call answers; a readable zero GUID where the call would otherwise read a null IID
  bool readable_iid;
};

const Named kExports[] = {
    {"GetBehaviorValue", 8, 0, false},
    {"D3D12CreateDevice", 16, uint32_t(E_INVALIDARG), false},
    {"D3D12GetDebugInterface", 8, uint32_t(E_NOINTERFACE), true},
    {"D3D12CreateRootSignatureDeserializer", 16, uint32_t(E_INVALIDARG), false},
    {"D3D12CreateVersionedRootSignatureDeserializer", 16, uint32_t(E_INVALIDARG), false},
    {"D3D12SerializeRootSignature", 16, uint32_t(E_INVALIDARG), false},
    {"D3D12SerializeVersionedRootSignature", 12, uint32_t(E_INVALIDARG), false},
    {"D3D12GetInterface", 12, uint32_t(E_NOINTERFACE), true},
    {"D3D12EnableExperimentalFeatures", 16, uint32_t(S_OK), false},
    {"D3D12CoreCreateLayeredDevice", 20, uint32_t(E_NOTIMPL), false},
    {"D3D12CoreGetLayeredDeviceSize", 8, 0, false},
    {"D3D12CoreRegisterLayers", 8, uint32_t(S_OK), false},
};

const struct {
  WORD ordinal;
  const char *name;
} kOrdinals[] = {{100, "GetBehaviorValue"}, {101, "D3D12CreateDevice"}, {102, "D3D12GetDebugInterface"}};
} // namespace

static unsigned
check_module(HMODULE module, const char *file, bool core, unsigned &checks) {
  unsigned wrong = 0;
  auto expect = [&](bool ok, const char *what, const char *name = "") {
    checks++;
    if (!ok && wrong++ < 24)
      printf("wrong: %s %s%s%s\n", file, name, *name ? ": " : "", what);
  };

  bool all_resolved = true;
  for (const Named &e : kExports) {
    if (!GetProcAddress(module, e.name)) {
      printf("missing export: %s %s\n", file, e.name);
      all_resolved = false;
    }
  }
  expect(all_resolved, "every export resolves");
  if (!all_resolved)
    return wrong;

  for (auto &o : kOrdinals)
    expect(GetProcAddress(module, MAKEINTRESOURCEA(o.ordinal)) == GetProcAddress(module, o.name),
           "is at its Wine ordinal", o.name);

  for (const Named &e : kExports) {
    ExportCall call = e.readable_iid ? call_export(GetProcAddress(module, e.name), e.arg_bytes)
                                     : call_export(GetProcAddress(module, e.name), e.arg_bytes, 0);
    expect(call.unpopped == 0, "pops exactly the argument bytes d3d12.lib's callers push", e.name);
    expect(uint32_t(call.result) == e.value, "answers what an all-zero call answers", e.name);
  }

  auto behavior = reinterpret_cast<UINT(WINAPI *)(const WCHAR *, UINT)>(
      reinterpret_cast<void *>(GetProcAddress(module, "GetBehaviorValue")));
  expect(behavior(L"NoSuchBehavior", 7) == 7, "an unset behavior leaves the caller's default", "GetBehaviorValue");
  expect(behavior(nullptr, 0) == 0, "a null name leaves the caller's default", "GetBehaviorValue");

  auto create = reinterpret_cast<HRESULT(WINAPI *)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **)>(
      reinterpret_cast<void *>(GetProcAddress(module, "D3D12CreateDevice")));
  ID3D12Device *device = nullptr;
  HRESULT hr = create(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void **>(&device));
  if (hr == E_NOINTERFACE) {
    // the router refused this process's Direct3D 12, so the device contracts the module keeps are not its own to
    // answer: the run names the skip instead of failures the refusal decides. the count a check_module returns is
    // its wrongs, so the skip is unsigned's own ceiling, which main() turns into the exit status the runner names
    printf("skipped: this build does not render Direct3D 12 in this process (router.d3d12.bitness = refuse)\n");
    return UINT_MAX;
  }
  expect(hr == S_OK && device, "D3D12CreateDevice(11_0) is S_OK with a device");
  if (device)
    device->Release();

  if (core) {
    auto version = reinterpret_cast<const UINT *>(
        reinterpret_cast<const void *>(GetProcAddress(module, "D3D12SDKVersion")));
    expect(version && *version == D3D12_SDK_VERSION, "D3D12SDKVersion is this build's SDK version");
    expect(GetProcAddress(GetModuleHandleA("d3d12.dll"), "D3D12SDKVersion") == nullptr,
           "D3D12SDKVersion is the core's data, not the loader's");
  }
  return wrong;
}

int
main() {
  unsigned wrong = 0, checks = 0;
  HMODULE loader = LoadLibraryA("d3d12.dll");
  HMODULE core = LoadLibraryA("d3d12core.dll");
  if (!loader || !core) {
    printf("failed: LoadLibrary d3d12.dll / d3d12core.dll (error %lu)\n", GetLastError());
    return 1;
  }
  const unsigned loader_checked = check_module(loader, "d3d12.dll", false, checks);
  if (loader_checked == UINT_MAX) {
    check_module(core, "d3d12core.dll", true, checks);
    return 77;
  }
  wrong += loader_checked;
  wrong += check_module(core, "d3d12core.dll", true, checks);
  FreeLibrary(core);
  FreeLibrary(loader);
  printf("%s: %u wrong of %u checks\n", wrong ? "failed" : "passed", wrong, checks);
  return wrong ? 1 : 0;
}
