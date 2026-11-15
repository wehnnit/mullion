// contract: stream output after a tessellator writes the domain shader's primitives (or, with a geometry shader, its
// own) as a list, in the API's order across draw instances and patches (D3D11.3 functional spec, 14.5), each element
// to its place in the vertex at the buffer's BufferFilledSize, which moves on by what was written, so the next draw
// appends. the order of a patch's own primitives is the tessellator's to choose, so the expectation is a patch's
// primitives as a set: a quad patch at integer factor N is N x N cells of two triangles each, clockwise, each vertex
// on the grid with the domain shader's outputs for its domain location, and the patch's constant (an instance and
// patch tag) the same on all of them. two draws append, the second at another factor; one pipeline rasterizes the
// stream as well, one has a geometry shader that passes triangles on and tags them, and a statistics query counts
// what was written.
#include "d3d12_test.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>

static const char hlsl[] = R"hlsl(
cbuffer c : register(b0) { float factor; };
struct CP { float2 corner : CORNER; uint tag : TAG; };
CP vs(uint id : SV_VertexID, uint instance : SV_InstanceID) {
  CP o;
  o.corner = float2(id & 1, (id >> 1) & 1);
  o.tag = instance * 16 + (id >> 2);
  return o;
}
struct HCP { float4 pos : POS; uint tag : TAG; };
struct PC {
  float edges[4] : SV_TessFactor;
  float inside[2] : SV_InsideTessFactor;
};
PC pc(OutputPatch<HCP, 4> p) {
  PC o;
  [unroll] for (int i = 0; i < 4; i++)
    o.edges[i] = factor;
  o.inside[0] = o.inside[1] = factor;
  return o;
}
[domain("quad")] [partitioning("integer")] [outputtopology("triangle_cw")] [outputcontrolpoints(4)]
[patchconstantfunc("pc")]
HCP hs(InputPatch<CP, 4> ip, uint i : SV_OutputControlPointID) {
  HCP o;
  o.pos = float4(ip[i].corner * float2(2, -2) + float2(-1, 1), 0, 1);
  o.tag = ip[i].tag;
  return o;
}
struct DSO {
  float4 pos : SV_Position;
  float2 where : WHERE;
  float2 f : F;
  nointerpolation uint value : VALUE;
};
[domain("quad")]
DSO ds(PC pc, float2 uv : SV_DomainLocation, const OutputPatch<HCP, 4> p) {
  DSO o;
  o.pos = lerp(lerp(p[0].pos, p[1].pos, uv.x), lerp(p[2].pos, p[3].pos, uv.x), uv.y);
  o.where = o.pos.xy;
  o.f = uv * uv;
  o.value = p[0].tag;
  return o;
}
// the triangles on, their values 1000 higher
[maxvertexcount(3)]
void gs(triangle DSO v[3], inout TriangleStream<DSO> s) {
  for (uint i = 0; i < 3; i++) {
    DSO o = v[i];
    o.value += 1000;
    s.Append(o);
  }
}
uint ps() : SV_Target { return 1; }
)hlsl";

int
main(int argc, char **argv) {
  Compiler compiler;
  if (!front_end(argc, argv, compiler)) {
    printf("skipped: dxcompiler.dll not found\n");
    return 77;
  }
  auto vs = compiler.compile(hlsl, "vs", "vs"), hs = compiler.compile(hlsl, "hs", "hs"),
       ds = compiler.compile(hlsl, "ds", "ds"), gs = compiler.compile(hlsl, "gs", "gs"),
       ps = compiler.compile(hlsl, "ps", "ps");
  for (auto *code : {&vs, &hs, &ds, &gs, &ps})
    if (code->empty()) {
      printf("failed: HLSL did not compile\n");
      return 1;
    }

  const UINT size = 32;
  ComPtr<ID3D12Device> device;
  CHECK(make_device(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
  D3D12_ROOT_PARAMETER param{D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS};
  param.Constants.Num32BitValues = 1;
  auto rs = root_signature(device.Get(), {1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT});
  if (!rs) {
    printf("failed: root signature\n");
    return 1;
  }

  // WHERE and F, a gap, then VALUE, in a 24-byte vertex
  const D3D12_SO_DECLARATION_ENTRY entries[] = {
      {0, "WHERE", 0, 0, 2, 0}, {0, "F", 0, 0, 2, 0}, {0, nullptr, 0, 0, 1, 0}, {0, "VALUE", 0, 0, 1, 0}
  };
  const UINT strides[] = {24}, stride = 24;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rs.Get();
  desc.VS = bytecode(vs);
  desc.HS = bytecode(hs);
  desc.DS = bytecode(ds);
  desc.StreamOutput = {entries, (UINT)std::size(entries), strides, 1, D3D12_SO_NO_RASTERIZED_STREAM};
  desc.SampleMask = ~0u;
  desc.RasterizerState = {D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE};
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
  desc.SampleDesc = {1, 0};
  ComPtr<ID3D12PipelineState> plain_pso, geometry_pso, raster_pso;
  step("pipeline with a hull and domain shader and stream output");
  CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&plain_pso)));
  step("pipeline with a geometry shader as well");
  desc.GS = bytecode(gs);
  CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&geometry_pso)));
  step("pipeline that rasterizes the stream");
  desc.GS = {};
  desc.PS = bytecode(ps);
  desc.StreamOutput.RasterizedStream = 0;
  desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = DXGI_FORMAT_R32_UINT;
  CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&raster_pso)));

  // the draws: two patches over two instances at factor 4, then a patch at factor 2. each group is a draw instance's
  // patch: its factor, tag and the filter on the first vertex's tag a geometry shader adds
  struct Draw {
    UINT vertices, instances;
    float factor;
  };
  const Draw draws[] = {{8, 2, 4}, {4, 1, 2}};
  struct Group {
    UINT factor, tag;
  };
  std::vector<Group> groups;
  for (auto &d : draws)
    for (UINT instance = 0; instance < d.instances; instance++)
      for (UINT patch = 0; patch < d.vertices / 4; patch++)
        groups.push_back({(UINT)d.factor, instance * 16 + patch});
  UINT64 triangles = 0;
  for (auto &g : groups)
    triangles += 2 * g.factor * g.factor;
  const UINT64 bytes = 3 * stride * triangles, guard = 3 * stride, sentinel = 0xa5a5a5a5u;
  const UINT64 region = bytes + guard, plain_at = 0, geometry_at = plain_at + region, raster_at = geometry_at + region,
               filled_at = raster_at + region, total = filled_at + 3 * 8;
  auto out = buffer(device.Get(), D3D12_HEAP_TYPE_DEFAULT, total, D3D12_RESOURCE_STATE_COPY_DEST);
  auto upload = buffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD, total, D3D12_RESOURCE_STATE_GENERIC_READ);
  auto readback = buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, total, D3D12_RESOURCE_STATE_COPY_DEST);
  uint8_t *init;
  CHECK(upload->Map(0, nullptr, (void **)&init));
  std::fill_n(reinterpret_cast<uint32_t *>(init), filled_at / 4, (uint32_t)sentinel);
  memset(init + filled_at, 0, 3 * 8);
  upload->Unmap(0, nullptr);

  ComPtr<ID3D12Resource> target;
  D3D12_HEAP_PROPERTIES default_heap{D3D12_HEAP_TYPE_DEFAULT};
  D3D12_RESOURCE_DESC target_desc{D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, size, size, 1, 1, DXGI_FORMAT_R32_UINT, {1, 0},
                                  D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};
  CHECK(device->CreateCommittedResource(
      &default_heap, D3D12_HEAP_FLAG_NONE, &target_desc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&target)
  ));
  ComPtr<ID3D12DescriptorHeap> rtvs;
  D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
  CHECK(device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtvs)));
  auto rtv = rtvs->GetCPUDescriptorHandleForHeapStart();
  device->CreateRenderTargetView(target.Get(), nullptr, rtv);
  auto pixels = buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * size, D3D12_RESOURCE_STATE_COPY_DEST);

  ComPtr<ID3D12QueryHeap> statistics;
  D3D12_QUERY_HEAP_DESC statistics_desc{D3D12_QUERY_HEAP_TYPE_SO_STATISTICS, 1};
  CHECK(device->CreateQueryHeap(&statistics_desc, IID_PPV_ARGS(&statistics)));
  auto statistics_out = buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, sizeof(D3D12_QUERY_DATA_SO_STATISTICS), D3D12_RESOURCE_STATE_COPY_DEST);

  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  D3D12_COMMAND_QUEUE_DESC queue_desc{D3D12_COMMAND_LIST_TYPE_DIRECT};
  CHECK(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));
  CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
  CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), plain_pso.Get(), IID_PPV_ARGS(&list)));
  list->CopyBufferRegion(out.Get(), 0, upload.Get(), 0, total);
  transition(list.Get(), out.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_STREAM_OUT);
  auto base = out->GetGPUVirtualAddress();
  const float clear[4] = {};
  list->ClearRenderTargetView(rtv, clear, 0, nullptr);
  D3D12_VIEWPORT viewport{0, 0, (float)size, (float)size, 0, 1};
  D3D12_RECT scissor{0, 0, (LONG)size, (LONG)size};
  list->SetGraphicsRootSignature(rs.Get());
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &scissor);
  list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
  auto record = [&](ID3D12PipelineState *pso, UINT64 at, UINT filled_index) {
    list->SetPipelineState(pso);
    D3D12_STREAM_OUTPUT_BUFFER_VIEW view{base + at, region, base + filled_at + 8 * filled_index};
    list->SOSetTargets(0, 1, &view);
    for (auto &d : draws) {
      list->SetGraphicsRoot32BitConstants(0, 1, &d.factor, 0);
      list->DrawInstanced(d.vertices, d.instances, 0, 0);
    }
  };
  list->BeginQuery(statistics.Get(), D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);
  record(plain_pso.Get(), plain_at, 0);
  list->EndQuery(statistics.Get(), D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);
  record(geometry_pso.Get(), geometry_at, 1);
  record(raster_pso.Get(), raster_at, 2);
  list->ResolveQueryData(statistics.Get(), D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1, statistics_out.Get(), 0);
  transition(list.Get(), out.Get(), D3D12_RESOURCE_STATE_STREAM_OUT, D3D12_RESOURCE_STATE_COPY_SOURCE);
  list->CopyBufferRegion(readback.Get(), 0, out.Get(), 0, total);
  transition(list.Get(), target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION pixels_dst{pixels.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT},
      pixels_src{target.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
  pixels_dst.PlacedFootprint = {0, {DXGI_FORMAT_R32_UINT, size, size, 1, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT}};
  list->CopyTextureRegion(&pixels_dst, 0, 0, 0, &pixels_src, nullptr);
  CHECK(list->Close());
  step("tessellated draws with stream output");
  CHECK(forget(readback.Get()));
  CHECK(forget(statistics_out.Get()));
  CHECK(forget(pixels.Get()));
  CHECK(execute(device.Get(), queue.Get(), list.Get()));

  uint8_t *got;
  CHECK(readback->Map(0, nullptr, (void **)&got));
  unsigned failures = 0, checks = 0;
  auto check = [&](const char *what, double value, double want, bool ok) {
    checks++;
    if (!ok && failures++ < 12)
      printf("%s: %g, want %g\n", what, value, want);
  };
  auto word = [&](UINT64 at) { return *reinterpret_cast<const uint32_t *>(got + at); };
  auto real = [&](UINT64 at) { return *reinterpret_cast<const float *>(got + at); };
  const float tolerance = 1e-3f;
  struct Region {
    const char *name;
    UINT64 at;
    UINT tag_added;
  };
  const Region regions[] = {{"domain shader", plain_at, 0}, {"geometry shader", geometry_at, 1000}, {"rasterized", raster_at, 0}};
  for (UINT r = 0; r < 3; r++) {
    auto &region_info = regions[r];
    unsigned before = failures;
    UINT64 filled = word(filled_at + 8 * r);
    check("filled size", filled, bytes, filled == bytes);
    for (UINT64 v = 3 * triangles; v < 3 * triangles + 3; v++)
      // past what was written, the guard is untouched
      for (UINT c = 0; c < stride / 4; c++) {
        auto got_word = word(region_info.at + v * stride + 4 * c);
        check("past the written vertices", got_word, sentinel, got_word == sentinel);
      }
    UINT64 first = 0;
    for (size_t g = 0; g < groups.size(); g++) {
      const UINT factor = groups[g].factor, count = 2 * factor * factor;
      std::map<std::pair<int, int>, int> cells;
      for (UINT t = 0; t < count; t++) {
        float x[3], y[3];
        int iu[3], iv[3];
        for (UINT k = 0; k < 3; k++) {
          UINT64 at = region_info.at + (first + 3 * t + k) * stride;
          x[k] = real(at), y[k] = real(at + 4);
          float u = (x[k] + 1) / 2, v = (1 - y[k]) / 2;
          iu[k] = (int)std::lround(u * factor), iv[k] = (int)std::lround(v * factor);
          check("vertex on the grid", u * factor, iu[k], std::abs(u * factor - iu[k]) <= tolerance * factor);
          check("vertex on the grid", v * factor, iv[k], std::abs(v * factor - iv[k]) <= tolerance * factor);
          float fu = (float)iu[k] / factor, fv = (float)iv[k] / factor;
          check("F.x", real(at + 8), fu * fu, std::abs(real(at + 8) - fu * fu) <= tolerance);
          check("F.y", real(at + 12), fv * fv, std::abs(real(at + 12) - fv * fv) <= tolerance);
          check("gap", word(at + 16), sentinel, word(at + 16) == sentinel);
          check("VALUE", word(at + 20), groups[g].tag + region_info.tag_added,
                word(at + 20) == groups[g].tag + region_info.tag_added);
        }
        // a half cell, clockwise (negative where y is up), within one cell
        float cross = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        float half_cell = 2.0f / (factor * factor);
        check("signed area", cross / 2, -half_cell, std::abs(cross / 2 + half_cell) <= tolerance * half_cell);
        int span_u = *std::max_element(iu, iu + 3) - *std::min_element(iu, iu + 3),
            span_v = *std::max_element(iv, iv + 3) - *std::min_element(iv, iv + 3);
        check("triangle spans cells", std::max(span_u, span_v), 1, span_u <= 1 && span_v <= 1);
        cells[{(iu[0] + iu[1] + iu[2]) / 3, (iv[0] + iv[1] + iv[2]) / 3}]++;
      }
      for (UINT cv = 0; cv < factor; cv++)
        for (UINT cu = 0; cu < factor; cu++)
          check("triangles in a cell", cells[{(int)cu, (int)cv}], 2, cells[{(int)cu, (int)cv}] == 2);
      first += 3 * count;
    }
    if (failures != before)
      printf("%s: the stream's contents differ\n", region_info.name);
  }
  D3D12_QUERY_DATA_SO_STATISTICS *counted;
  CHECK(statistics_out->Map(0, nullptr, (void **)&counted));
  check("primitives written", counted->NumPrimitivesWritten, triangles, counted->NumPrimitivesWritten == triangles);
  check("primitives needed", counted->PrimitivesStorageNeeded, triangles, counted->PrimitivesStorageNeeded == triangles);
  uint8_t *drawn;
  CHECK(pixels->Map(0, nullptr, (void **)&drawn));
  for (UINT y = 0; y < size; y++)
    for (UINT x = 0; x < size; x++) {
      auto pixel = reinterpret_cast<const uint32_t *>(drawn + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * y)[x];
      check("rasterized pixel", pixel, 1, pixel == 1);
    }
  readback->Unmap(0, nullptr);
  statistics_out->Unmap(0, nullptr);
  pixels->Unmap(0, nullptr);
  printf("%s: %u wrong of %u checks\n", failures ? "failed" : "passed", failures, checks);
  return failures ? 1 : 0;
}
