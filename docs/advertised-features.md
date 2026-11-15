# Advertised feature edges

Reproduce against current source. Historical conformance JSON is a lead, not proof. If a path falls back to ordinary raster, either implement the tier or stop advertising it.

| Feature | Advertised | Status | Last checked |
|---|---|---|---|
| Depth bounds | yes | implemented; MSAA / mesh / tess / UAV side effects not re-run this session | not run |
| Variable rate shading tier 2 | yes | implemented; inner coverage / mesh not re-run this session | not run |
| Conservative raster tier 3 | yes | implemented; fallback to ordinary raster not proven absent | not run |
| Sparse / tiled 3D | tier 3 | Metal tile shapes differ from D3D standard (`tests/d3d12/d3d12_tiled3d.cpp`) | tests exist |
| Tessellation + stream output | tessellation yes; combined SO yes | `CreateGraphicsPipelineState` builds stream output after the tessellator and writes domain shader primitives, with or without a geometry shader (`src/d3d12/d3d12_pipeline_graphics.cpp`, `tests/d3d12/d3d12_streamout_tess.cpp`, `tests/d3d12/d3d12_streamout_tessellation.cpp`) | source and tests |
| Non-raster SO with resource-writing shaders | SO yes | existing stream-out tests; resource-writing non-raster not re-run this session | tests exist |
| SM 6.7 | yes | advanced texture ops advertised TRUE; WriteableMSAA stays FALSE (Metal `texture2d_ms` read-only). Unhandled DXBC opcodes on valid input return a structured `SM50` error instead of asserting (`tests/d3d12/d3d12_sm67.cpp`, `tests/d3d12/d3d12_commandlist8.cpp`) | source and tests |

Host for any future re-run: record macOS, GPU, Wine, Mullion commit, and the exact `tests/run.py` family.

This pass (Apple M4 Pro, macOS 26.5.2, wine-8.17, `buildfresh`): tessellation plus stream output is created and covered by `tests/d3d12/d3d12_streamout_tess.cpp` and `tests/d3d12/d3d12_streamout_tessellation.cpp`. Depth bounds / VRS / conservative raster were **not re-run** beyond existing D3D11/D3D12 tests already in the tree. Sparse 3D still differs from D3D tile shapes (`tests/d3d12/d3d12_tiled3d.cpp`). Unhandled DXBC instructions now fail `SM50Initialize` with a message. M1/M2 hardware: **not run**.
