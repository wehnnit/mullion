#include "Metal.hpp"
#include "airconv_public.h"
#include "d3d11_private.h"
#include "d3d11_pipeline.hpp"
#include "d3d11_device.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"
#include <atomic>
#include <mutex>

namespace mullion {
// A depth-only draw has no pixel shader. This one writes D3D's depth: the bias of 15.10, clamped to the viewport,
// and 24-bit unorm rounded before it is stored. Metal's own bias is left at 0 for the draw. aliased lines are quads,
// and only the diamond-exit pixels of that quad keep the depth (D3D11.3 3.4.4).
WMT::Function
DepthCorrectFunction(WMT::Device device, bool diamond, bool depth_max) {
  static std::mutex mutex;
  static WMT::Reference<WMT::Library> library;
  static WMT::Reference<WMT::Function> plain, lines, peaked;
  std::lock_guard<std::mutex> lock(mutex);
  if (library)
    return depth_max ? peaked : diamond ? lines : plain;
  const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct DepthFix { float bias; float scale; float clampv; float bits; float apply; float pad; float vmin; float vmax; };
struct DepthOut { float depth [[depth(any)]]; };
// float32's z * (2^24-1) rounds 0.75 to the next code, so the multiply is done in integers.
// float(code) / float(2^24-1) then lands one ulp high for some codes, and the unorm pack reads code + 1
static uint quant_d24(float z) {
  uint code = 0u;
  if (z > 0.0f && z < 1.0f) {
    uint bits = as_type<uint>(z);
    int exp = int((bits >> 23) & 255u) - 127;
    int shift = 23 - exp;
    if ((bits >> 23) != 0u && shift >= 1 && shift <= 62) {
      ulong mant = ulong(bits & 0x7fffffu) | 0x800000ul;
      ulong rounded = (mant * 16777215ul + (1ul << uint(shift - 1))) >> uint(shift);
      code = uint(min(rounded, 16777215ul));
    }
  } else if (z >= 1.0f) {
    code = 16777215u;
  }
  return code;
}
static float encode_d24(float z) {
  uint code = quant_d24(z);
  float stored = float(code) / 16777215.0f;
  if (quant_d24(stored) != code)
    stored = nextafter(stored, 0.0f);
  return stored;
}
static float corrected(float4 position, constant DepthFix &c, float magnitude) {
  float z = position.z;
  if (c.apply != 0.0f) {
    float slope = max(abs(dfdx(z)), abs(dfdy(z)));
    float unit;
    if (c.bits == 0.0f) {
      // float depth's unit is constant for the primitive: the greatest |window z|, not this pixel's (15.10)
      float m = magnitude;
      float exponent = m == 0.0f ? -126.0f : floor(log2(m));
      unit = exp2(exponent - 23.0f);
    } else {
      unit = 1.0f / (exp2(c.bits) - 1.0f);
    }
    float scaled = c.scale != 0.0f ? c.scale * slope : c.scale;
    float value = c.bias * unit + scaled;
    if (c.clampv > 0.0f)
      value = min(c.clampv, value);
    else if (c.clampv < 0.0f)
      value = max(c.clampv, value);
    z += value;
  }
  z = fmin(c.vmax, fmax(c.vmin, z));
  if (c.bits == 24.0f)
    z = encode_d24(z);
  return z;
}
[[fragment]] DepthOut fs_correct_depth(float4 position [[position]], constant DepthFix &c [[buffer(20)]]) {
  DepthOut out;
  out.depth = corrected(position, c, abs(position.z));
  return out;
}
struct DepthPeak {
  float4 position [[position]];
  float max_abs [[user(mullion_depth_max), flat]];
};
[[fragment]] DepthOut fs_correct_depth_max(DepthPeak in [[stage_in]], constant DepthFix &c [[buffer(20)]]) {
  DepthOut out;
  out.depth = corrected(in.position, c, in.max_abs);
  return out;
}
// the quad is two pixels wide; the diamond, not the quad, is the line (tests/raster.hpp diamond()).
// fast math misses the boundary equality the rule decides on
#pragma clang fp reassociate(off) contract(off)
static bool diamond_hit(float2 p, float4 a, float4 b) {
  float dx = b.x - a.x, dy = b.y - a.y;
  if (dx == 0.0f && dy == 0.0f)
    return false;
  float lo = 0.0f, hi = 1.0f;
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sy = -1; sy <= 1; sy += 2) {
      float d = float(sx) * (a.x - p.x) + float(sy) * (a.y - p.y) - 0.5f;
      float sl = float(sx) * dx + float(sy) * dy;
      if (sl == 0.0f) {
        if (d > 0.0f || (d == 0.0f && sy < 0))
          return false;
      } else if (sl > 0.0f) {
        hi = min(hi, -d / sl);
      } else {
        lo = max(lo, -d / sl);
      }
    }
  auto inside = [&](float t) {
    float u = a.x + t * dx - p.x, v = a.y + t * dy - p.y, manh = abs(u) + abs(v);
    // the boundary is exact in the reference's double math; float misses it by an ulp and drops the pixel
    if (manh < 0.5f)
      return true;
    if (abs(manh - 0.5f) > 1e-4f)
      return false;
    return v > 0.0f || (abs(u - 0.5f) <= 1e-4f && abs(dy) > abs(dx));
  };
  return lo <= hi && hi >= 0.0f && (hi < 1.0f || !inside(1.0f)) && inside((lo + hi) * 0.5f);
}
struct LineEnds {
  float4 position [[position]];
  float4 a [[user(mullion_line_a), flat]];
  float4 b [[user(mullion_line_b), flat]];
};
[[fragment]] DepthOut fs_correct_depth_line(LineEnds ends [[stage_in]], constant DepthFix &c [[buffer(20)]]) {
  DepthOut out;
  if (!diamond_hit(ends.position.xy, ends.a, ends.b))
    discard_fragment();
  out.depth = corrected(ends.position, c, abs(ends.position.z));
  return out;
}
)";
  WMT::Reference<WMT::Error> error;
  library = device.newLibraryWithSource(source, error);
  if (!library)
    return {};
  plain = library.newFunction("fs_correct_depth");
  lines = library.newFunction("fs_correct_depth_line");
  peaked = library.newFunction("fs_correct_depth_max");
  return depth_max ? peaked : diamond ? lines : plain;
}

class MTLCompiledGraphicsPipelineImpl
    : public MTLCompiledGraphicsPipeline {
public:
  MTLCompiledGraphicsPipelineImpl(MTLD3D11Device *pDevice,
                              MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : num_rtvs(pDesc->NumColorAttachments),
        depth_stencil_format(pDesc->DepthStencilFormat),
        topology_class(pDesc->TopologyClass), device_(pDevice),
        pBlendState(pDesc->BlendState),
        RasterizationEnabled(pDesc->RasterizationEnabled),
        SampleCount(pDesc->SampleCount),
        diamond_lines_(pDesc->DiamondLines) {
    uint32_t unorm_output_reg_mask = 0;
    for (unsigned i = 0; i < num_rtvs; i++) {
      rtv_formats[i] = pDesc->ColorAttachmentFormats[i];
      unorm_output_reg_mask |= (uint32_t(IsUnorm8RenderTargetFormat(pDesc->ColorAttachmentFormats[i])) << i);
    }

    VertexShader = pDesc->VertexShader->get_shader(ShaderVariantVertex{
        pDesc->InputLayout, pDesc->GSPassthrough, !pDesc->RasterizationEnabled, pDesc->ClipDistances != 0});

    if (pDesc->PixelShader) {
      PixelShader = pDesc->PixelShader->get_pixel_shader(ShaderVariantPixel{
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          depth_stencil_format == WMTPixelFormatInvalid,
          unorm_output_reg_mask, pDesc->PipelineStatistics, pDesc->BlendState->IsAlphaToCoverage(),
          pDesc->CenteredSamples, pDesc->SingleRasterSample, pDesc->PairedSamples, pDesc->ClipDistances,
          pDesc->DepthUnormBits, pDesc->DiamondLines, pDesc->PlaneSlope},
          {rtv_formats, num_rtvs}, pDesc->BlendState);
      ps_valid_render_targets = pDesc->PixelShader->reflection().PSValidRenderTargets;
    } else {
      PixelShader = nullptr;
      ps_valid_render_targets = 0;
    }
  }

  void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *pPipeline, bool statistics) final {
    if (statistics && description->PixelShader && !description->PipelineStatistics) {
      auto desc = *description;
      desc.PipelineStatistics = true;
      MTLCompiledGraphicsPipeline *pipeline;
      device_->CreateGraphicsPipeline(&desc, &pipeline);
      return pipeline->GetPipeline(pPipeline);
    }
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_};
  }

  ThreadpoolWork *RunThreadpoolWork() {

    TRACE("Start compiling 1 PSO");

    WMT::Reference<WMT::Error> err;
    MTL_COMPILED_SHADER vs, ps;
    if (!VertexShader->GetShader(&vs)) {
      return VertexShader;
    }
    if (PixelShader && !PixelShader->GetShader(&ps)) {
      return PixelShader;
    }

    WMTRenderPipelineInfo info;
    WMT::InitializeRenderPipelineInfo(info);

    info.vertex_function = vs.Function;

    if (PixelShader) {
      info.fragment_function = ps.Function;
    } else if (depth_stencil_format != WMTPixelFormatInvalid && RasterizationEnabled) {
      info.fragment_function = DepthCorrectFunction(device_->GetMTLDevice(), diamond_lines_, false);
    }
    info.rasterization_enabled = RasterizationEnabled;

    for (unsigned i = 0; i < num_rtvs; i++) {
      if (rtv_formats[i] == WMTPixelFormatInvalid)
        continue;
      info.colors[i].pixel_format = rtv_formats[i];
    }

    if (depth_stencil_format != WMTPixelFormatInvalid) {
      info.depth_pixel_format = depth_stencil_format;
    }
    if (DepthStencilPlanarFlags(depth_stencil_format) & 2) {
      info.stencil_pixel_format = depth_stencil_format;
    }

    if (pBlendState) {
      pBlendState->SetupMetalPipelineDescriptor((WMTRenderPipelineBlendInfo *)&info, num_rtvs, ps_valid_render_targets);
    }
    // explicit shader coverage disables A2C (D3D11.3 17.18)
    if (description->PixelShader && description->PixelShader->reflection().PixelShader.HasCoverageOutput)
      info.alpha_to_coverage_enabled = false;

    info.input_primitive_topology = topology_class;
    info.raster_sample_count = SampleCount;
    info.immutable_vertex_buffers = (1 << 16) | (1 << 29) | (1 << 30);
    info.immutable_fragment_buffers = (1 << 29) | (1 << 30);

    state_ = device_->GetMTLDevice().newRenderPipelineState(info, err);

    if (state_ == nullptr) {
      ERR("Failed to create PSO: ", err.description().getUTF8String());
      return this;
    }

    TRACE("Compiled 1 PSO");

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) {
    ready_.store(state);
    ready_.notify_all();
  }

private:
  UINT num_rtvs;
  UINT ps_valid_render_targets;
  WMTPixelFormat rtv_formats[8];
  WMTPixelFormat depth_stencil_format;
  WMTPrimitiveTopologyClass topology_class;
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  CompiledShader *VertexShader;
  CompiledShader *PixelShader;
  IMTLD3D11BlendState *pBlendState;
  WMT::Reference<WMT::RenderPipelineState> state_;
  bool RasterizationEnabled;
  UINT SampleCount;
  bool diamond_lines_;
};

std::unique_ptr<MTLCompiledGraphicsPipeline>
CreateGraphicsPipeline(MTLD3D11Device *pDevice,
                       MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  return std::make_unique<MTLCompiledGraphicsPipelineImpl>(pDevice, pDesc);
}

class MTLCompiledComputePipelineImpl
    : public MTLCompiledComputePipeline {
public:
  MTLCompiledComputePipelineImpl(MTLD3D11Device *pDevice, ManagedShader shader)
      : device_(pDevice) {
    ComputeShader = shader->get_shader(ShaderVariantDefault{});
    uint32_t total_tgsize = shader->reflection().ThreadgroupSize[0] *
                            shader->reflection().ThreadgroupSize[1] *
                            shader->reflection().ThreadgroupSize[2];
    // FIXME: might be different on AMD GPU, if it's ever supported
    tgsize_is_multiple_of_sgwidth = (total_tgsize % 32) == 0;
  }

  void GetPipeline(MTL_COMPILED_COMPUTE_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_};
  }

  ThreadpoolWork *RunThreadpoolWork() {

    TRACE("Start compiling 1 PSO");

    WMT::Reference<WMT::Error> err;
    MTL_COMPILED_SHADER cs;
    if (!ComputeShader->GetShader(&cs)) {
      return ComputeShader;
    }

    WMTComputePipelineInfo info;
    WMT::InitializeComputePipelineInfo(info);

    info.compute_function = cs.Function;
    info.tgsize_is_multiple_of_sgwidth = tgsize_is_multiple_of_sgwidth;
    info.immutable_buffers = (1 << 29) | (1 << 30);

    state_ = device_->GetMTLDevice().newComputePipelineState(info, err);

    if (!state_) {
      ERR("Failed to create compute PSO: ", err.description().getUTF8String());
      return this;
    }

    TRACE("Compiled 1 PSO");

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) {
    ready_.store(state);
    ready_.notify_all();
  }

private:
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  CompiledShader *ComputeShader;
  WMT::Reference<WMT::ComputePipelineState> state_;
  bool tgsize_is_multiple_of_sgwidth;
};

std::unique_ptr<MTLCompiledComputePipeline>
CreateComputePipeline(MTLD3D11Device *pDevice, ManagedShader ComputeShader) {
  return std::make_unique<MTLCompiledComputePipelineImpl>(pDevice, ComputeShader);
}

} // namespace mullion