// contract: stream output after a tessellator writes the domain shader's primitives, or the geometry shader's after
// it, as lists in the API's order of patches (draw instance, then patch), each patch's primitives together. inside a
// patch "no explicit order is prescribed" for the tessellator's primitives (D3D11.3 functional spec, 11.5), so a patch's
// are checked as a set: a quad patch at integer factor N, edges and inside alike, is (N + 1)^2 domain points as
// points, or 2 N^2 triangles, two to each of its N x N cells. a geometry shader's instances of a primitive follow it
// (primitive, geometry instance, then emission). the factor goes 1 to 3 with the patch and instance, over enough
// patches and two instances that the draws span several object threadgroups. writes start at the buffer's
// BufferFilledSize, which moves on by what was written, so a second draw appends; a stream stops at the first
// primitive that does not fit whole (14.5), here partway through a patch. a statistics query counts the triangles
// written and needed, the triangles rasterize stream 0 over the whole target, and an ExecuteIndirect DRAW streams the
// points out as the direct draw does. the list runs twice, the second replaying the first.
#include "d3d12_test.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <set>

static const char hlsl[] = R"hlsl(
struct CP { float2 corner : CORNER; uint instance : INSTANCE; };
CP vs(uint id : SV_VertexID, uint instance : SV_InstanceID) {
  CP o;
  o.corner = float2(id & 1, (id >> 1) & 1);
  o.instance = instance;
  return o;
}
struct PC {
  float edges[4] : SV_TessFactor;
  float inside[2] : SV_InsideTessFactor;
  uint id : ID;
  uint factor : FACTOR;
};
PC pc(InputPatch<CP, 4> ip, uint patch : SV_PrimitiveID) {
  PC o;
  uint factor = 1 + (patch + ip[0].instance) % 3;
  [unroll] for (int i = 0; i < 4; i++)
    o.edges[i] = factor;
  o.inside[0] = o.inside[1] = factor;
  o.id = patch + ip[0].instance * 1000;
  o.factor = factor;
  return o;
}
[domain("quad")] [partitioning("integer")] [outputtopology("point")] [outputcontrolpoints(4)]
[patchconstantfunc("pc")] [maxtessfactor(3)]
CP hs_points(InputPatch<CP, 4> ip, uint i : SV_OutputControlPointID) { return ip[i]; }
[domain("quad")] [partitioning("integer")] [outputtopology("triangle_cw")] [outputcontrolpoints(4)]
[patchconstantfunc("pc")] [maxtessfactor(3)]
CP hs_triangles(InputPatch<CP, 4> ip, uint i : SV_OutputControlPointID) { return ip[i]; }
struct O { float4 pos : SV_Position; uint4 value : VALUE; };
// the patch, the domain point on the patch's grid, the factor; the patch covers clip space
[domain("quad")]
O ds(PC pc, float2 uv : SV_DomainLocation, const OutputPatch<CP, 4> p) {
  O o;
  o.pos = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
  o.value = uint4(pc.id, uint(round(uv.x * pc.factor)), uint(round(uv.y * pc.factor)), pc.factor);
  return o;
}
// for each instance a point of each triangle: its patch, the instance, its cell, the factor
[maxvertexcount(1)] [instance(2)]
void gs(triangle O v[3], uint i : SV_GSInstanceID, inout PointStream<O> s) {
  uint2 low = min(min(v[0].value.yz, v[1].value.yz), v[2].value.yz);
  O o;
  o.pos = float4(0, 0, 0, 1);
  o.value = uint4(v[0].value.x, i, low.x + (low.y << 8), v[0].value.w);
  s.Append(o);
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
  auto vs = compiler.compile(hlsl, "vs", "vs"), hs_points = compiler.compile(hlsl, "hs_points", "hs"),
       hs_triangles = compiler.compile(hlsl, "hs_triangles", "hs"), ds = compiler.compile(hlsl, "ds", "ds"),
       gs = compiler.compile(hlsl, "gs", "gs"), ps = compiler.compile(hlsl, "ps", "ps");
  for (auto *code : {&vs, &hs_points, &hs_triangles, &ds, &gs, &ps})
    if (code->empty()) {
      printf("failed: HLSL did not compile\n");
      return 1;
    }
  ComPtr<ID3D12Device> device;
  CHECK(make_device(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
  auto rs = root_signature(device.Get(), {0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT});

  const D3D12_SO_DECLARATION_ENTRY entries[] = {{0, "VALUE", 0, 0, 4, 0}};
  const UINT strides[] = {16}, size = 4;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{rs.Get(), bytecode(vs)};
  desc.HS = bytecode(hs_points);
  desc.DS = bytecode(ds);
  desc.SampleMask = ~0u;
  desc.RasterizerState = {D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE};
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
  desc.SampleDesc = {1, 0};
  desc.StreamOutput = {entries, 1, strides, 1, D3D12_SO_NO_RASTERIZED_STREAM};
  ComPtr<ID3D12PipelineState> points_pso, gs_pso, triangles_pso;
  CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&points_pso)));
  desc.HS = bytecode(hs_triangles);
  desc.GS = bytecode(gs);
  CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&gs_pso)));
  // the domain shader's triangles, rasterized too
  desc.GS = {};
  desc.StreamOutput.RasterizedStream = 0;
  desc.PS = bytecode(ps);
  desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = DXGI_FORMAT_R32_UINT;
  CHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&triangles_pso)));

  // the patches in the API's order, with their factors
  const UINT patches = 40, instances = 2;
  struct Patch {
    uint32_t id, factor;
  };
  std::vector<Patch> order;
  for (UINT instance = 0; instance < instances; instance++)
    for (UINT p = 0; p < patches; p++)
      order.push_back({p + instance * 1000, 1 + (p + instance) % 3});
  UINT64 point_count = 0, triangle_count = 0;
  for (auto &p : order)
    point_count += (p.factor + 1) * (p.factor + 1), triangle_count += 2 * p.factor * p.factor;
  // the small buffer stops partway through the third patch
  const UINT64 fits = (order[0].factor + 1) * (order[0].factor + 1) + (order[1].factor + 1) * (order[1].factor + 1) + 3;

  // one buffer: the points of two draws, the small buffer, the triangles' vertices, the geometry shader's points
  // (two per triangle), the indirect draw's points; the filled sizes
  const UINT64 sentinel = 0xa5a5a5a5u;
  const UINT64 points_at = 0, points_bytes = 16 * point_count * 2, small_at = points_at + points_bytes,
               small_bytes = 16 * fits + 8, triangles_at = small_at + small_bytes, triangles_bytes = 16 * 3 * triangle_count,
               gs_at = triangles_at + triangles_bytes, gs_bytes = 16 * 2 * triangle_count, indirect_at = gs_at + gs_bytes,
               indirect_bytes = 16 * point_count, filled_at = indirect_at + indirect_bytes, total = filled_at + 8 * 5;
  auto out = buffer(device.Get(), D3D12_HEAP_TYPE_DEFAULT, total, D3D12_RESOURCE_STATE_COPY_DEST);
  auto upload = buffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD, total, D3D12_RESOURCE_STATE_GENERIC_READ);
  auto readback = buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, total, D3D12_RESOURCE_STATE_COPY_DEST);
  uint8_t *init;
  CHECK(upload->Map(0, nullptr, (void **)&init));
  std::fill_n(reinterpret_cast<uint32_t *>(init), filled_at / 4, sentinel);
  // filled sizes, each with a guard word after it
  for (UINT i = 0; i < 5; i++)
    reinterpret_cast<uint32_t *>(init + filled_at)[2 * i] = 0, reinterpret_cast<uint32_t *>(init + filled_at)[2 * i + 1] = sentinel;

  auto args = buffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(D3D12_DRAW_ARGUMENTS), D3D12_RESOURCE_STATE_GENERIC_READ);
  const D3D12_DRAW_ARGUMENTS draw_args{4 * patches, instances, 0, 0};
  void *mapped;
  CHECK(args->Map(0, nullptr, &mapped));
  memcpy(mapped, &draw_args, sizeof(draw_args));
  args->Unmap(0, nullptr);
  D3D12_INDIRECT_ARGUMENT_DESC operation{D3D12_INDIRECT_ARGUMENT_TYPE_DRAW};
  D3D12_COMMAND_SIGNATURE_DESC sig_desc{sizeof(draw_args), 1, &operation};
  ComPtr<ID3D12CommandSignature> signature;
  CHECK(device->CreateCommandSignature(&sig_desc, nullptr, IID_PPV_ARGS(&signature)));

  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  D3D12_COMMAND_QUEUE_DESC queue_desc{D3D12_COMMAND_LIST_TYPE_DIRECT};
  CHECK(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));
  CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
  CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), points_pso.Get(), IID_PPV_ARGS(&list)));
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
  ComPtr<ID3D12QueryHeap> statistics;
  D3D12_QUERY_HEAP_DESC statistics_desc{D3D12_QUERY_HEAP_TYPE_SO_STATISTICS, 1};
  CHECK(device->CreateQueryHeap(&statistics_desc, IID_PPV_ARGS(&statistics)));
  auto statistics_out =
      buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, sizeof(D3D12_QUERY_DATA_SO_STATISTICS), D3D12_RESOURCE_STATE_COPY_DEST);
  auto pixels = buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * size, D3D12_RESOURCE_STATE_COPY_DEST);

  list->CopyBufferRegion(out.Get(), 0, upload.Get(), 0, total);
  transition(list.Get(), out.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_STREAM_OUT);
  auto base = out->GetGPUVirtualAddress();
  auto view = [&](UINT64 at, UINT64 bytes, UINT filled_index) {
    return D3D12_STREAM_OUTPUT_BUFFER_VIEW{base + at, bytes, base + filled_at + 8 * filled_index};
  };
  const float clear[4] = {};
  list->ClearRenderTargetView(rtv, clear, 0, nullptr);
  D3D12_VIEWPORT viewport{0, 0, (float)size, (float)size, 0, 1};
  D3D12_RECT scissor{0, 0, (LONG)size, (LONG)size};
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &scissor);
  list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  list->SetGraphicsRootSignature(rs.Get());
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
  // the points twice into one buffer, then into the small one
  auto points_view = view(points_at, points_bytes, 0), small_view = view(small_at, small_bytes, 1),
       triangles_view = view(triangles_at, triangles_bytes, 2), gs_view = view(gs_at, gs_bytes, 3),
       indirect_view = view(indirect_at, indirect_bytes, 4);
  list->SOSetTargets(0, 1, &points_view);
  list->DrawInstanced(4 * patches, instances, 0, 0);
  list->DrawInstanced(4 * patches, instances, 0, 0);
  list->SOSetTargets(0, 1, &small_view);
  list->DrawInstanced(4 * patches, instances, 0, 0);
  list->SOSetTargets(0, 1, &indirect_view);
  list->ExecuteIndirect(signature.Get(), 1, args.Get(), 0, nullptr, 0);
  list->SetPipelineState(triangles_pso.Get());
  list->SOSetTargets(0, 1, &triangles_view);
  list->BeginQuery(statistics.Get(), D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);
  list->DrawInstanced(4 * patches, instances, 0, 0);
  list->EndQuery(statistics.Get(), D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);
  list->ResolveQueryData(statistics.Get(), D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1, statistics_out.Get(), 0);
  list->SetPipelineState(gs_pso.Get());
  list->SOSetTargets(0, 1, &gs_view);
  list->DrawInstanced(4 * patches, instances, 0, 0);
  transition(list.Get(), out.Get(), D3D12_RESOURCE_STATE_STREAM_OUT, D3D12_RESOURCE_STATE_COPY_SOURCE);
  list->CopyBufferRegion(readback.Get(), 0, out.Get(), 0, total);
  transition(list.Get(), target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION pixels_dst{pixels.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT},
      pixels_src{target.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
  pixels_dst.PlacedFootprint = {0, {DXGI_FORMAT_R32_UINT, size, size, 1, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT}};
  list->CopyTextureRegion(&pixels_dst, 0, 0, 0, &pixels_src, nullptr);
  transition(list.Get(), target.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK(list->Close());

  for (UINT replay : {0u, 1u}) {
    step("stream output after a tessellator, replay %u", replay);
    CHECK(forget(readback.Get()));
    CHECK(forget(statistics_out.Get()));
    CHECK(forget(pixels.Get()));
    CHECK(execute(device.Get(), queue.Get(), list.Get()));
    uint8_t *got;
    CHECK(readback->Map(0, nullptr, (void **)&got));
    unsigned failures = 0;
    auto expect = [&](const char *what, uint64_t value, uint64_t want) {
      if (value != want && failures++ < 12)
        printf("%s: %#llx, want %#llx\n", what, (unsigned long long)value, (unsigned long long)want);
    };
    auto word = [&](UINT64 at) { return *reinterpret_cast<const uint32_t *>(got + at); };
    using Record = std::array<uint32_t, 4>;
    auto record = [&](UINT64 at) { return Record{word(at), word(at + 4), word(at + 8), word(at + 12)}; };
    // a patch's points from `at`, `count` of them (all its grid's when `count` is past it): each its own, on the grid
    auto check_points = [&](const char *what, UINT64 at, const Patch &p, UINT64 count) {
      std::set<std::pair<uint32_t, uint32_t>> seen;
      for (UINT64 n = 0; n < count; n++) {
        auto r = record(at + 16 * n);
        expect(what, r[0], p.id);
        expect(what, r[3], p.factor);
        if (r[1] > p.factor || r[2] > p.factor || !seen.insert({r[1], r[2]}).second)
          expect(what, (uint64_t(r[1]) << 32) | r[2], ~0ull);
      }
      return count * 16;
    };
    auto grid = [](const Patch &p) { return UINT64(p.factor + 1) * (p.factor + 1); };
    // the points of a draw from `at`, patch after patch
    auto check_draw = [&](const char *what, UINT64 at) {
      for (auto &p : order)
        at += check_points(what, at, p, grid(p));
      return at;
    };
    auto after = check_draw("points, first draw", points_at);
    after = check_draw("points, appended draw", after);
    expect("points filled", word(filled_at), points_bytes);
    UINT64 at = small_at;
    for (UINT i = 0; i < 2; i++)
      at += check_points("small buffer", at, order[i], grid(order[i]));
    at += check_points("small buffer, last patch", at, order[2], 3);
    expect("small buffer past the fit", word(at), sentinel);
    expect("small buffer filled", word(filled_at + 8), 16 * fits);
    check_draw("indirect points", indirect_at);
    expect("indirect points filled", word(filled_at + 8 * 4), indirect_bytes);
    // each triangle's vertices on one cell of its patch, each cell under two
    at = triangles_at;
    for (auto &p : order) {
      std::map<uint32_t, uint32_t> cells;
      for (uint32_t t = 0; t < 2 * p.factor * p.factor; t++, at += 48) {
        uint32_t low[2] = {~0u, ~0u}, high[2] = {0, 0};
        for (int k = 0; k < 3; k++) {
          auto r = record(at + 16 * k);
          expect("triangle vertex patch", r[0], p.id);
          expect("triangle vertex factor", r[3], p.factor);
          for (int c = 0; c < 2; c++)
            low[c] = std::min(low[c], r[1 + c]), high[c] = std::max(high[c], r[1 + c]);
        }
        expect("triangle within a cell", high[0] - low[0] == 1 && high[1] - low[1] == 1, 1);
        cells[low[0] + (low[1] << 8)]++;
      }
      expect("cells of a patch", cells.size(), p.factor * p.factor);
      for (auto [cell, count] : cells)
        expect("triangles of a cell", count, 2);
    }
    expect("triangles filled", word(filled_at + 16), triangles_bytes);
    // the geometry shader's two points of each triangle, one after the other
    at = gs_at;
    for (auto &p : order) {
      std::map<uint32_t, uint32_t> cells;
      for (uint32_t t = 0; t < 2 * p.factor * p.factor; t++, at += 32) {
        auto first = record(at), second = record(at + 16);
        expect("geometry point patch", first[0], p.id);
        expect("geometry point factor", first[3], p.factor);
        expect("geometry instance 0", first[1], 0);
        expect("geometry instance 1", second[1], 1);
        expect("geometry instances' patch", second[0], p.id);
        expect("geometry instances' cell", second[2], first[2]);
        cells[first[2]]++;
      }
      expect("cells of a patch, geometry shader", cells.size(), p.factor * p.factor);
      for (auto [cell, count] : cells)
        expect("triangles of a cell, geometry shader", count, 2);
    }
    expect("geometry points filled", word(filled_at + 24), gs_bytes);
    for (UINT i = 0; i < 5; i++)
      expect("word after a filled size", word(filled_at + 8 * i + 4), sentinel);
    D3D12_QUERY_DATA_SO_STATISTICS *counted;
    CHECK(statistics_out->Map(0, nullptr, (void **)&counted));
    expect("triangles written", counted->NumPrimitivesWritten, triangle_count);
    expect("triangles needed", counted->PrimitivesStorageNeeded, triangle_count);
    uint8_t *drawn;
    CHECK(pixels->Map(0, nullptr, (void **)&drawn));
    for (UINT y = 0; y < size; y++)
      for (UINT x = 0; x < size; x++)
        expect("drawn pixel", reinterpret_cast<const uint32_t *>(drawn + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * y)[x], 1);
    readback->Unmap(0, nullptr);
    statistics_out->Unmap(0, nullptr);
    pixels->Unmap(0, nullptr);
    if (failures) {
      printf("failed: %u wrong values\n", failures);
      return 1;
    }
  }
  printf("passed: %llu points and %llu triangles per draw over %zu patches\n", (unsigned long long)point_count,
         (unsigned long long)triangle_count, order.size());
  return 0;
}
