// contract: what a DLSS title does with nvngx.dll, done through the DLL's own export table rather than through
// Mullion's headers. A game reaches Deep Learning Super Resolution by loading nvngx.dll, resolving
// NVSDK_NGX_D3D12_* by name, asking what the device can do, creating a feature on a command list and evaluating it
// every frame (NVIDIA NGX SDK, "NGX D3D12 API"): GetCapabilityParameters hands back a parameter bag that says
// whether the optimal-settings callback is there, GetFeatureRequirements answers for one feature id, and
// CreateFeature refuses when the bag lacks a size, a format or a command list it cannot record on.
// Checked here: every name a title imports resolves out of the loaded DLL; supersampling is reported supported and
// an id Mullion does not implement is reported unsupported rather than supported; a feature is created on a real
// command list, records an upscale, and is released; and a create whose parameters are incomplete is refused.
// The parameter bag and the feature ids come from src/nvngx/nvngx.hpp: it is the interface NVIDIA ships, and a test
// that re-declared it by hand would be checking its own guess at the vtable instead of the DLL's answer.
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include <cstring>
#include "../../src/nvngx/nvngx.hpp"
#include "../trace.hpp"
#include "d3d12_test.hpp"

namespace {

using NgxParameter = NVNGXParameter;
using CreateFn = unsigned(ID3D12GraphicsCommandList *, unsigned, NgxParameter *, unsigned **);
using EvaluateFn = unsigned(ID3D12GraphicsCommandList *, const unsigned *, NgxParameter *, void *);
using CapabilitiesFn = unsigned(NgxParameter **);
using RequirementsFn = unsigned(void *, const void *, void *);
using AllocateFn = unsigned(NgxParameter **);
using DestroyFn = void(NgxParameter *);
using ReleaseFn = void(unsigned *);

struct FeatureRequirement {
  unsigned supported;
  unsigned min_hw_architecture;
  char min_os_version[255];
};
struct FeatureDiscoveryInfo {
  unsigned sdk_version;
  unsigned feature_id;
  char identifier[32];
  const wchar_t *application_data_path;
  const void *feature_info;
};

constexpr unsigned kFeatureSupersampling = NVNGX_FEATURE_SUPERSAMPLING;
constexpr unsigned kFeatureRayReconstruction = NVNGX_FEATURE_RAYRECONSTRUCTION;
constexpr unsigned kFeatureUnimplemented = 27; // no NGX feature this number
constexpr unsigned kResultOk = NVNGX_RESULT_OK, kSupported = NVNGX_FEATURE_SUPPORT_RESULT_SUPPORTED;
constexpr unsigned kFlagMvLowRes = NVNGX_DLSS_FLAG_MV_LOWRES;

// GetProcAddress answers a FARPROC, which no C++ cast takes to a real function type. Fn names the function, so this
// hands back a pointer to it
template <typename Fn> Fn *symbol(HMODULE module, const char *name) {
  return reinterpret_cast<Fn *>(reinterpret_cast<void *>(GetProcAddress(module, name)));
}

} // namespace

extern "C" int main(int argc, char **argv) {
  // NVEXT is what makes Mullion's NGX core and the MetalFX bridge answer at all
  SetEnvironmentVariableA("MULLION_ENABLE_NVEXT", "1");
  HMODULE ngx = LoadLibraryExA("nvngx.dll", nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!ngx) {
    printf("failed: nvngx.dll did not load (%lu)\n", GetLastError());
    return 1; // no verdict: nothing was measured
  }
  // every name a DLSS title imports
  struct Named { const char *name; };
  static const Named kImports[] = {
      "NVSDK_NGX_D3D12_Init",           "NVSDK_NGX_D3D12_Init_Ext",
      "NVSDK_NGX_D3D12_Init_with_ProjectID", "NVSDK_NGX_D3D12_Shutdown",
      "NVSDK_NGX_D3D12_GetCapabilityParameters", "NVSDK_NGX_D3D12_GetFeatureRequirements",
      "NVSDK_NGX_D3D12_AllocateParameters", "NVSDK_NGX_D3D12_DestroyParameters",
      "NVSDK_NGX_D3D12_CreateFeature",  "NVSDK_NGX_D3D12_EvaluateFeature",
      "NVSDK_NGX_D3D12_EvaluateFeature_C", "NVSDK_NGX_D3D12_ReleaseFeature",
      "NVSDK_NGX_D3D12_GetScratchBufferSize", "NVSDK_NGX_D3D12_Shutdown1",
  };
  bool every_import = true;
  for (const Named &e : kImports)
    if (!GetProcAddress(ngx, e.name)) {
      every_import = false;
      printf("wrong: %s does not resolve out of the loaded nvngx.dll\n", e.name);
    }
  expect(every_import, "every import a DLSS title needs resolves");

  auto capabilities = symbol<CapabilitiesFn>(ngx, "NVSDK_NGX_D3D12_GetCapabilityParameters");
  auto requirements = symbol<RequirementsFn>(ngx, "NVSDK_NGX_D3D12_GetFeatureRequirements");
  auto allocate = symbol<AllocateFn>(ngx, "NVSDK_NGX_D3D12_AllocateParameters");
  auto destroy = symbol<DestroyFn>(ngx, "NVSDK_NGX_D3D12_DestroyParameters");
  auto create = symbol<CreateFn>(ngx, "NVSDK_NGX_D3D12_CreateFeature");
  auto evaluate = symbol<EvaluateFn>(ngx, "NVSDK_NGX_D3D12_EvaluateFeature");
  auto release = symbol<ReleaseFn>(ngx, "NVSDK_NGX_D3D12_ReleaseFeature");
  if (!capabilities || !requirements || !allocate || !destroy || !create || !evaluate || !release) {
    expect(false, "an entry point this test calls is missing");
    return verdict();
  }

  step("what the device is said to support");
  NgxParameter *caps = nullptr;
  expect(capabilities(&caps) == kResultOk && caps, "GetCapabilityParameters answers a parameter bag");
  if (caps) {
    void *callback = nullptr;
    expect(caps->Get("DLSSOptimalSettingsCallback", &callback) == kResultOk && callback,
          "the optimal-settings callback is offered");
    unsigned denoising = 0xffffffff;
    expect(caps->Get("SuperSamplingDenoising.Available", &denoising) == kResultOk,
          "the availability of Ray Reconstruction is readable");
  }

  FeatureDiscoveryInfo info{};
  info.sdk_version = 1;
  FeatureRequirement requirement{};
  info.feature_id = kFeatureSupersampling;
  memcpy(info.identifier, "DLSS", 5);
  expect(requirements(nullptr, &info, &requirement) == kResultOk &&
            requirement.supported == kSupported,
        "supersampling is reported supported");
  info.feature_id = kFeatureRayReconstruction;
  memcpy(info.identifier, "DLSS RR", 8);
  expect(requirements(nullptr, &info, &requirement) == kResultOk, "Ray Reconstruction answers a requirement");
  FeatureRequirement other{};
  info.feature_id = kFeatureUnimplemented;
  memcpy(info.identifier, "nope", 5);
  expect(requirements(nullptr, &info, &other) != kResultOk || other.supported != kSupported,
        "a feature Mullion does not implement is not reported supported");

  step("a feature created on a command list and evaluated");
  ComPtr<ID3D12Device> device;
  CHECK(make_device(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
  D3D12_COMMAND_QUEUE_DESC queue_desc{D3D12_COMMAND_LIST_TYPE_DIRECT};
  ComPtr<ID3D12CommandQueue> queue;
  CHECK(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));
  ComPtr<ID3D12CommandAllocator> allocator;
  CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
  ComPtr<ID3D12GraphicsCommandList> list;
  CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));

  auto texture = [&](DXGI_FORMAT format, UINT w, UINT h) -> ComPtr<ID3D12Resource> {
    ComPtr<ID3D12Resource> resource;
    D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC desc{D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, w, h, 1, 1, format, {1, 0},
                             D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
    // not CHECK: that returns from main with an int, and this lambda answers a ComPtr
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                               IID_PPV_ARGS(&resource))))
      expect(false, "no %ux%u texture for the feature's input", w, h);
    return resource;
  };
  auto input = texture(DXGI_FORMAT_R16G16B16A16_FLOAT, 64, 40), output = texture(DXGI_FORMAT_R8G8B8A8_UNORM, 128, 80);
  auto depth = texture(DXGI_FORMAT_R32_FLOAT, 64, 40), vectors = texture(DXGI_FORMAT_R16G16_FLOAT, 64, 40);

  NgxParameter *params = nullptr;
  expect(allocate(&params) == kResultOk && params, "AllocateParameters answers a bag");
  if (!params)
    return verdict();
  params->Set("Width", 64u);
  params->Set("Height", 40u);
  params->Set("OutWidth", 128u);
  params->Set("OutHeight", 80u);
  params->Set("PerfQualityValue", 2u); // NVSDK_NGX_PerfQuality_Value_MaxQuality
  params->Set("DLSS.Feature.Create.Flags", (unsigned)kFlagMvLowRes);
  unsigned *feature = nullptr;
  const unsigned created = create(list.Get(), kFeatureSupersampling, params, &feature);
  expect(created == kResultOk && feature, "CreateFeature makes a supersampling feature on a command list");

  params->Set("Color", input.Get());
  params->Set("Output", output.Get());
  params->Set("Depth", depth.Get());
  params->Set("MotionVectors", vectors.Get());
  params->Set("Jitter.Offset.X", 0.3f);
  params->Set("Jitter.Offset.Y", 0.15f);
  params->Set("MV.Scale.X", 1.f);
  params->Set("MV.Scale.Y", 1.f);
  params->Set("DLSS.Pre.Exposure", 1.f);
  if (feature) {
    expect(evaluate(list.Get(), feature, params, nullptr) == kResultOk, "EvaluateFeature records an upscale");
    // the frame the upscale was recorded in has to reach the GPU: what a title does every frame, and what a
    // recorded-but-never-submitted list would let a broken bridge pass
    expect(SUCCEEDED(submit(device.Get(), queue.Get(), list.Get())), "the frame with the upscale in it runs");
    release(feature);
  }

  step("a create whose parameters are incomplete is refused");
  NgxParameter *incomplete = nullptr;
  allocate(&incomplete);
  incomplete->Set("Width", 0u);
  incomplete->Set("Height", 0u);
  unsigned *no_feature = reinterpret_cast<unsigned *>(1);
  const unsigned refused = create(list.Get(), kFeatureSupersampling, incomplete, &no_feature);
  expect(refused != kResultOk, "a feature with no extent is not created");
  destroy(incomplete);

  destroy(params);
  return verdict();
}
