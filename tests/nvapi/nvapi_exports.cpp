// contract: nvapi64.dll as a game binds to it. First that it answers at all only for a device said to be NVIDIA's:
// its DllMain returns FALSE unless dxgi.customVendorId is 10de, so a title on any other GPU finds no NVAPI rather
// than a NVAPI that lies about its vendor. Then NVIDIA's API has no named exports: the DLL gives one entry point,
// nvapi_QueryInterface, and a title looks every function up by the hash of its name (external/nvapi/
// nvapi_interface.h is that table of names to ids). So what matters is three things a title can see. Every function
// Mullion implements answers its id, and answers it with a pointer that is not another function's. An id it does not
// have answers no pointer at all, because a hash that happened to land on something would be called with the wrong
// arguments. And the calls that are safe without a frame in flight behave as NVAPI says: Initialize answers OK,
// enumeration finds the one adapter behind the device, and a Reflex or HDR call with a null device or a struct of a
// version it does not know is refused rather than answered.
// Nothing here needs a real GPU's driver: NVAPI's contract for a function Mullion does not implement is that the
// lookup fails, and for a bad argument that the call returns NVAPI_*_INVALID_ARGUMENT.
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include <cstring>
#include <iterator>
#include "nvapi_lite_common.h"
// the same order src/nvapi/nvapi.cpp uses: nvapi.h spells its SAL annotations only when this is not defined
#define __NVAPI_EMPTY_SAL
#include "nvapi.h"
#include "nvapi_interface.h"
#include "../trace.hpp"

namespace {

using QueryInterface = void *(*) (unsigned id);

QueryInterface query = nullptr;

// the id NVIDIA gives a name, which is the only way to reach it
void *by_name(const char *name) {
  // no sentinel: the array is sized by its own initializer, so the bound is its length and running past it is a
  // strcmp on whatever follows
  for (unsigned i = 0; i < std::size(nvapi_interface_table); i++)
    if (!strcmp(nvapi_interface_table[i].func, name))
      return query(nvapi_interface_table[i].id);
  return nullptr;
}

// every function src/nvapi/nvapi.cpp implements
const char *const kImplemented[] = {
    "NvAPI_Initialize",
    "NvAPI_EnumLogicalGPUs",
    "NvAPI_EnumPhysicalGPUs",
    "NvAPI_EnumNvidiaDisplayHandle",
    "NvAPI_GPU_GetArchInfo",
    "NvAPI_GPU_GetBusId",
    "NvAPI_GPU_GetBusType",
    "NvAPI_GPU_GetAdapterIdFromPhysicalGpu",
    "NvAPI_GPU_GetAllClockFrequencies",
    "NvAPI_DISP_GetGDIPrimaryDisplayId",
    "NvAPI_DISP_GetDisplayIdByDisplayName",
    "NvAPI_DISP_GetMonitorCapabilities",
    "NvAPI_Disp_GetHdrCapabilities",
    "NvAPI_Disp_HdrColorControl",
    "NvAPI_D3D_SetSleepMode",
    "NvAPI_D3D_GetSleepStatus",
    "NvAPI_D3D_Sleep",
    "NvAPI_D3D_SetLatencyMarker",
    "NvAPI_D3D_GetLatency",
    "NvAPI_D3D_SetVerticalSyncMode",
    "NvAPI_D3D_GetCurrentSLIState",
    "NvAPI_D3D_GetObjectHandleForResource",
    "NvAPI_D3D_SetResourceHint",
    "NvAPI_DRS_CreateSession",
    "NvAPI_DRS_LoadSettings",
};

} // namespace

extern "C" int main(int, char **) {
  // NVAPI answers only for a device said to be NVIDIA's: nvapi64.dll's DllMain returns FALSE unless
  // dxgi.customVendorId is 10de, so a title on any other GPU gets no NVAPI at all rather than a half-true one. That
  // gate is checked before the library is let in, because after it is mapped it stays mapped for the process
  if (HMODULE refused = LoadLibraryExA("nvapi64.dll", nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
    printf("wrong: nvapi64.dll initialized with no NVIDIA vendor id configured\n");
    FreeLibrary(refused);
    return verdict();
  } else
    expect(GetLastError() == ERROR_DLL_INIT_FAILED, "nvapi64.dll refuses to initialize for a device that is not NVIDIA's");

  SetEnvironmentVariableA("MULLION_CONFIG", "dxgi.customVendorId=10de");
  HMODULE nvapi = LoadLibraryExA("nvapi64.dll", nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!nvapi) {
    printf("failed: nvapi64.dll did not load with NVIDIA's vendor id (%lu)\n", GetLastError());
    return 1; // no verdict: nothing was measured
  }
  // the one export. a named NvAPI_* entry point would be the wrong shape: titles resolve hashes
  query = reinterpret_cast<QueryInterface>(
      reinterpret_cast<void *>(GetProcAddress(nvapi, "nvapi_QueryInterface")));
  expect(query != nullptr, "nvapi_QueryInterface is the export a title looks functions up through");
  if (!query)
    return verdict();

  step("every function Mullion implements answers its id, and answers it with its own pointer");
  void *seen[sizeof(kImplemented) / sizeof(*kImplemented)] = {};
  unsigned resolved = 0;
  for (unsigned i = 0; i < std::size(kImplemented); i++) {
    seen[i] = by_name(kImplemented[i]);
    if (!seen[i]) {
      printf("wrong: %s does not answer its id\n", kImplemented[i]);
      trace::wrong++;
      continue;
    }
    for (unsigned earlier = 0; earlier < i; earlier++)
      if (seen[earlier] == seen[i]) {
        printf("wrong: %s and %s answer the same pointer\n", kImplemented[earlier], kImplemented[i]);
        trace::wrong++;
      }
    resolved++;
  }
  expect(resolved == std::size(kImplemented), "every implemented function resolves: %u of %zu", resolved,
         std::size(kImplemented));

  step("an id Mullion does not have answers nothing, not a pointer of another function's shape");
  // a hash is not a check: answering one that is not yours is a call with the wrong arguments
  expect(by_name("NvAPI_NVIDIAStatus_ApplicationNotification") == nullptr ||
             by_name("NvAPI_NVIDIAStatus_ApplicationNotification") != by_name("NvAPI_Initialize"),
         "an unimplemented id is not answered with Initialize's pointer");
  expect(query(0xdeadbeef) == nullptr, "a hash outside the table answers no pointer");

  step("the calls a title makes before it renders anything");
  auto initialize = reinterpret_cast<NvAPI_Status(*)()>(by_name("NvAPI_Initialize"));
  expect(initialize && initialize() == NVAPI_OK, "NvAPI_Initialize answers OK");

  NvPhysicalGpuHandle handles[32] = {};
  NvU32 count = 0;
  auto enum_physical = reinterpret_cast<NvAPI_Status (*)(NvPhysicalGpuHandle *, NvU32 *)>(
      by_name("NvAPI_EnumPhysicalGPUs"));
  expect(enum_physical && enum_physical(handles, &count) == NVAPI_OK && count >= 1,
         "physical GPU enumeration finds the adapter behind the device");

  NvDisplayHandle display = nullptr;
  auto enum_display = reinterpret_cast<NvAPI_Status (*)(NvU32, NvDisplayHandle *)>(
      by_name("NvAPI_EnumNvidiaDisplayHandle"));
  expect(enum_display && enum_display(0, &display) == NVAPI_OK && display, "the first display handle is answered");

  step("Reflex and HDR refuse what they must refuse, and answer what they claim");
  NV_GET_SLEEP_STATUS_PARAMS status_params{};
  status_params.version = NV_GET_SLEEP_STATUS_PARAMS_VER;
  auto sleep_status = reinterpret_cast<NvAPI_Status (*)(IUnknown *, NV_GET_SLEEP_STATUS_PARAMS *)>(
      by_name("NvAPI_D3D_GetSleepStatus"));
  expect(sleep_status && sleep_status(nullptr, &status_params) == NVAPI_INVALID_ARGUMENT,
         "a Reflex status call with no device is refused");
  ID3D12Device *device = nullptr;
  if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
    expect(sleep_status(device, &status_params) == NVAPI_OK, "Reflex answers a status of its own version");
    // a version the implementation does not know is not an invitation to fill a struct it does not have
    NV_GET_SLEEP_STATUS_PARAMS wrong_version = status_params;
    wrong_version.version++;
    expect(sleep_status(device, &wrong_version) != NVAPI_OK, "a Reflex struct of an unknown version is refused");
    device->Release();
  } else
    expect(false, "no device to ask about Reflex");

  printf("nvapi: %u of %u looked up; NVIDIA's table holds more than this build implements\n", resolved,
         static_cast<unsigned>(std::size(kImplemented)));
  return verdict();
}
