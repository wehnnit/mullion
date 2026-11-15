# Game-parity matrix

Template per title: executable/build, API, arch, Mullion commit + loaded DLL hashes, Wine/CrossOver, macOS/GPU/RAM, D3DMetal version, resolution/settings/upscaler, scene route, cold vs warm cache, visual/crash/hang, median/p95/p99, stutter, memory growth.

Parity target: same scene/settings/hardware as D3DMetal; report median/p95/p99 frame time ratio. Do not claim parity from a HUD feature level.

Optimization: #64 allocator-owned ICB pooling, #65 D3D12 parse intern, and D3D9 dirty constant tracking are in tree. #66 MetalFX config file and #70 category tracing stay env-gated and were not landed as a measured change. Nothing in this table is a D3DMetal frame-time ratio. #52 sincos range is unchanged.

## Seed titles

| Title | API | Arch | Status | Notes |
|---|---|---|---|---|
| Black Myth: Wukong | D3D12 | x64 | **not run** | README screenshots were shader-compile, not a warm-cache benchmark |
| God of War | D3D11 | x64 | **not run** | |
| God of War Ragnarök | D3D12 | x64 | **not run** | same as Wukong: compile-screen numbers are not a row |
| Red Dead Redemption 2 | D3D12 | x64 | **not run** | |
| Dragon's Dogma 2 | D3D12 | x64 | **not run** | |
| Marvel's Spider-Man Remastered | D3D12 | x64 | **not run** | |
| a UE5 title | D3D12 | x64 | **not run** | pick one actually installed |
| an RE Engine title | D3D12 | x64 | **not run** | Gears E-Day / RE9 only if installed |
| a D3D9 FF title | D3D9 | i386 or x64 | **not run** | enable_d3d9 build; log refusals via `MULLION_LOG_PATH` |
| a D3D9 SM2 title | D3D9 | i386 or x64 | **not run** | |
| a D3D9 SM3 title | D3D9 | i386 or x64 | **not run** | vs_3_0/ps_3_0 draws exist (`d3d9_ff_extra`); caps report 3.0. `d3d9_caps` draws a ps_1_1. Title not run. |

Untested titles stay unclaimed. APIs D3DMetal does not cover use native Windows (or another established impl) as reference.

Capability, not a game row: `D3D12_FEATURE_D3D12_OPTIONS14` reports `AdvancedTextureOpsSupported` TRUE, `IndependentFrontAndBackStencilRefMaskSupported` TRUE, and `WriteableMSAATexturesSupported` FALSE (`tests/d3d12/d3d12_commandlist8.cpp`, `tests/d3d12/d3d12_sm67.cpp`). Stream output after tessellation is `tests/d3d12/d3d12_streamout_tess.cpp` and `tests/d3d12/d3d12_streamout_tessellation.cpp`. Direct3D 9 caps report shader model 3.0, two fixed function blend stages, and table fog (`d3d9_caps`, `d3d9_ff_extra`, `d3d9_extra`). `d3d9.dll` exports the 17 modern names, including the `D3DPERF_*` calls (`d3d9_exports`). No title in the table was run for any of that.

## Validation environment (this implementation pass)

- Host: Apple M4 Pro, macOS 26.5.2 (25F84), arm64
- Wine: `toolchains/wine` (`wine-8.17`)
- Toolchain: `toolchains/llvm-mingw-20260908-ucrt-macos-universal`
- Baseline commit: `96dbae09` on `local/shared-resources`; this pass is uncommitted on top of that (`AIRCONV_VERSION` 85)
- Builds: `build32` (win32, enable_d3d9) and `buildfresh` (win64, enable_d3d9/d3d12)
- PE32+ gate (`build64-baseline`, `/tmp/gate_pe64_archknown.log`, families native and 9): 124 passed, 2 known (`d3d10_raster` ×2), 0 new. D3D9 filter all green on both families.
- PE32 gate (`build32-baseline`, `/tmp/gate_pe32_archknown.log`, family native, Microsoft i386 `d3dcompiler_47` via `--compilers`): 53 passed, 3 known (`d3d10_depth_stencil`, `d3d10_raster`, `d3d10_raster_bias` under `/i386`), 0 new. Every D3D9 test in the filter passed.
- Wave 4 checks on PE32+ native and family 9: `d3d11_border`, `d3d11_present_flags`, `d3d12_agility`, `d3d11_geometry_indirect_parts` passed; host `host_stream_range` and `host_typed_window` passed
- Games in the table above: **not run**
- M1/M2 hardware: **not run** (this host is M4 Pro; `supportsFamily` lowering is not a substitute)
- ARM64EC: **not run**
- D3DMetal frame-time ratios: **not run**
- Measured optimization: #64 ICB pool, #65 D3D12 parse intern, D3D9 dirty constants landed; `tests/perf` runner keeps every repeat/variance/failure. #66/#70 not landed. D3DMetal comparison **not run**.
