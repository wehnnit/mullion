# Mullion requirements matrix

Persistent status of the assignment. `Implemented` means the behaviour exists and a test exercises it; `Partial`
means some cases work and the gap is named; `Open` means no implementation yet. Evidence lives in
`MULLION_IMPLEMENTATION_REPORT.md` and `toolchains/d3d9-evidence/`.

## Direct3D 9 / 9Ex

| Requirement | Status | Evidence / gap |
| --- | --- | --- |
| Triangle list/strip and fan, all four draw paths | Implemented | `d3d9_fan` (28 checks): nonidentity indices, per-vertex colour, `StartIndex`, `BaseVertexIndex`, 16/32-bit, index values >65535, culling, scratch reuse |
| Point fill | Partial | one-pixel points drawn as `POINTLIST`; point size, `POINTSCALE*`, point sprites open |
| Four render targets | Implemented | `d3d9_topology`: `oC0..oC3` read independently; `d3d9_targets`: independent slots, `D3DERR_NOTFOUND`, larger depth accepted |
| Target/depth sample-count and size agreement, both binding orders | Implemented | `d3d9_targets`: 4× target refuses a 1× depth, 1× target refuses a 4× depth, failed calls leave the previous surface |
| Per-target colour-write masks and blending | Implemented | `d3d9_topology`: `D3DRS_COLORWRITEENABLE{,1,2,3}` independently mask four targets; NONE, ALL, single and multi-channel masks; alternating passes; state-block restore. `d3d9_state.cpp` builds an independent D3D11 blend state per target; caps set `D3DPMISCCAPS_INDEPENDENTWRITEMASKS` |
| Pixel samplers s0–s15 | Implemented | `d3d9_texture`, `d3d9_caps`; `d3d9_shader_draw` samples s15 in a ps_3_0 draw
| Vertex samplers 257–260 (`D3DVERTEXTEXTURESAMPLER0..3`, internally 16–19) | Implemented (rendering) | `d3d9_shader_draw` vs_3_0 `tex2Dlod` through each of 257–260 and simultaneous pixel stage 0; `d3d9_texture`/`d3d9_caps` public-id, refusal and state-block checks |
| Fixed-function pipeline (transforms, combiners, fog, lighting, clipping) | Partial | existing paths; two blender stages remain the cap; full combiner matrix and vertex blending incomplete. `XYZ|TEX1`/`XYZRHW|TEX1` no-diffuse draws pack to the 24-byte diffuse layout for a game PS, while 3/4-component texcoord FVFs stay on the passthrough path; per-link TEXCOORD0 base 2 (unlit FF) vs 3 (lit/game) |
| Shader models 1.x–3.0 | Partial | translator exists; full instruction/register audit and malformed-input matrix open |
| Streams, declarations/FVF, instancing, `ProcessVertices` | Partial | 16 streams and frequency implemented. Which draws the instance count applies to is now pinned against the reference rather than assumed: `DrawIndexedPrimitive` draws the instances the `D3DSTREAMSOURCE_INDEXEDDATA` stream names, and a non-indexed `DrawPrimitive` draws one instance whatever the frequency says (Wine `visual.c` stream_test: "Only a single instance should be drawn"), while the three refusals `wined3d_stateblock_set_stream_source_freq` makes (both flags, `INSTANCEDATA` on stream 0, a zero count) are the only ones — a plain frequency on any stream is accepted, as `stateblock.c:1679-1692` shows (`tests/d3d9/d3d9_streams.cpp`, `d3d9_ff_extra.cpp`, `d3d9_draw_contract.cpp`; 43/43 both ABIs at `78a795c64a982553` and `6ac9a80797567fed`); FVF conversion now follows D3D9's own table (`XYZ`/`XYZW`/`XYZRHW`, `XYZB1`–`XYZB5`, `LASTBETA_*`, per-texture sizes), declarations are validated (`E_FAIL` for a non-terminator `D3DDECLTYPE_UNUSED` or a misaligned offset), and SetFVF's declaration per format is cached with D3D9's pointer identity and application-only reference counts; the wider declaration-type matrix is open |
| Formats, pools, locking, managed/autogen mips, cube/volume/compressed | Partial | `d3d9_texture`, `d3d9_mips`, `d3d9_volume`, `d3d9_cube`; L8 and D3D9Ex user-memory `SYSTEMMEM` textures/surfaces work; signed V8U8/V16U16/Q8W8V8U8 sampling and packing pass except four Q8 alpha-boundary checks; general system-memory buffers, cube/volume system-memory resources and `GetDC` are open |
| Queries, state blocks, multithreaded/pure device | Partial | state blocks exist; vertex sampler entries added; full block coverage open |
| Lost device, `Reset`/`ResetEx`, fullscreen transitions, presentation flags, frame latency | Partial | reset and present tests exist; gamma/cursor and all intervals open |
| D3D9Ex sharing, residency, lifecycle | Partial | residency checks, creation-mode-correct Ex IIDs, swap-chain Ex methods, Ex depth-stencil usage, user-memory texture/surface paths and `D3DSPD_IUNKNOWN` private data exist; the Wine 8.17 `d3d9ex` entry now completes (1975 checks) with only its macOS window-rect check failing, and 30-back-buffer chains, frame latency, Ex pool rules, Ex `Reset` state retention and NULL focus-window devices conform. General shared/system-memory buffers, GDI interop and fullscreen/mode lifecycle remain open |
| Raster status / vertical blank | Partial | device estimate plus `IDXGIOutput::WaitForVBlank`; swap chain now shares the device measurement; no measured raster position exists on macOS |
| 9On12 device/queue/node interoperation | Open | overrides are refused and logged, not implemented |
| Shader validator ABI/behaviour | Partial | virtual methods occupy vtable slots; no PE32/PE64 call-through test |

## Direct3D 10 / 11

| Requirement | Status | Evidence / gap |
| --- | --- | --- |
| Shared D3D10/11 device, DXGI, presenter, shader converters | Partial | preexisting implementation. The surface a Direct3D 10 title asks the device for is now audited rather than assumed: `IUnknown`, `ID3D10Device`, `ID3D10Device1`, `ID3D10Multithread` (set, read back, and restored), `IDXGIDevice` and the DXGI parent chain, and a `ID3D10BlendState1` read through `GetDesc1` — 16 checks, 0 wrong on PE64 (`1f83689451dfc429`) and PE32 (`927557e784b6e47f`), `tests/d3d10/d3d10_surface.cpp`. `ID3D10InfoQueue` is required to be refused on a device made without the debug layer, as Wine's `d3d10_1.c:300` expects. The D3D10 *feature* failures (raster, raster_bias, depth_stencil, block_values) are the shared clusters in "The zero-failure campaign" and are still open |
| #150 R32 typeless UAV views | Implemented (preexisting) | `tests/d3d11/d3d11_view_creation.cpp`; preserved, not re-run here |
| Writable MSAA, format casting, advanced texture ops | Partial/Open | dirty-tree work exists; no new implementation |
| Full D3D11 suite regression | Not run this continuation | previous passes ran it; rerun needed before any claim |

## Direct3D 12

| Requirement | Status | Evidence / gap |
| --- | --- | --- |
| Descriptors/root signatures, barriers, queues/fences, bundles, ExecuteIndirect | Partial | preexisting |
| Tessellation, stream output, mesh/amplification, raytracing, VRS, conservative raster, depth bounds, sampler feedback | Partial | #147 typed descriptors and #148 `MULLION_D3D12_RAYTRACING=0` integrated; `d3d12_view_kinds` 2/2 and `d3d12_raytracing_off` 2/2 pass |
| PE32 D3D12 descriptor handles | Open | x86 build guard and `EMBEDDED_DESCRIPTOR_HANDLE` unresolved |
| Shader Model 6.9 / DirectX-Headers 1.620.1 audit | Open | not started |

## Architectures

| Target | Status | Evidence |
| --- | --- | --- |
| PE32+ x86_64 | Implemented and verified | D3D9 gate **37/37, 0 failed, 0 skipped** with `--validate` on `build64-2311` (`d3d9.dll` `269a818d145bf5e6`, llvm-mingw 20261104, wine-8.17), manifest `dee942e1763d9589` (`.agent-obj/logs/gate-pe64-d3d9-r3.log`, `gate-pe64-d3d9-validated.log`); the 7 November pass: `d3d12_ d3d9_` on `8fcc018…` **522 passed, 36 failed, 4 skipped** — 34 known, 2 new fingerprints matching the prior pass's own host-sensitive findings (`.agent-obj/logs/gate-pe64-r2.log`) |
| PE32 i386 | Implemented and verified | D3D9 gate **37/37, 0 failed, 0 skipped** with `--validate` on `build32-2311` (`d3d9.dll` `e9d304fe77194c37`), manifest `d304731ba579e80d` (`.agent-obj/logs/gate-pe32-d3d9-r3.log`, `gate-pe32-d3d9-validated.log`). Wine `stateblock` 0 real failures, `device` 10 (display geometry), `d3d9ex` 3 (2 window-rect, 1 the Wine `broken()` Present case) (`.agent-obj/logs/conf-*-r111.log`) |
| PE32 Direct3D 12 | **Runs, does not render; the router can refuse it** | `d3d12_` on `8fcc018…`: **284 passed, 238 failed, 4 skipped** (`.agent-obj/logs/gate-pe32-d3d12-final.log`). Built with a 128-slot table behind the 32-bit descriptor handle (`src/d3d12/d3d12_descriptor_heap.hpp`), no 128-bit integers on i386, and every CPU-read or CPU-written buffer carrying memory the 32-bit side allocated (`CpuPlaced`), because Metal maps a buffer it made itself above 4 GB. The remaining failures are the rendering-correctness class (draws encode identically to PE64 — the record streams match — but the GPU-side data path for 32-bit still produces cleared-target readbacks) plus assorted single-test defects. `router.d3d12.bitness = refuse` (mullion.conf) exists to answer those titles with the error a fallback expects instead of a device that draws the clear, and is **off by default**: the 283 `d3d12_` tests each create their device with a `CHECK`, so refusing by default turns 283 results into unclear failures rather than the named skips docs/TESTING.md 6.2 requires. Turning it on is the step after those tests ask for a device through a helper that skips (`tests/host/router.cpp` holds the rule and its 30 checks; `tests/dxgi/dxgi_router.cpp` the 16 checks of what a title sees) |
| ARM64EC / ARM64X | Open | not built or run |
| Mixed-architecture sharing | Open | |

## Dependencies and upstream

| Item | Status | Gap |
| --- | --- | --- |
| Upstream main `93de32b` | Integrated locally | #147 and #148 applied semantically on `d0e58d9`; no push |
| AIRCONV_VERSION 108 vs 99 | Resolved | combined converter is 109; old caches are rejected |
| llvm-mingw 20260908 → 20261104 | **Bumped, verified** | `toolchains/llvm-mingw-20261104-ucrt-macos-universal` (clang 23.1.5) built `build64-2311` and `build32-2311`; native D3D9 **37/37** on PE64 and PE32 on it (`.agent-obj/logs/gate-pe64-d3d9-r3.log`, `gate-pe32-d3d9-r3.log`; the 36-test runs that preceded the 37th test are `toolchain-2311-pe64-d3d9.log`, `toolchain-2311-pe32-d3d9.log`), and the full PE64 suite on it is `.agent-obj/logs/final2-pe64-full.log` (706/38/5, 36 known) |
| DXMT `647b0020` | Open | not ported |
| DXC v1.9.2609.5, DirectX-Headers 1.620.1 | DXC current; headers blocked | the prefix already carries DXC **1.9.2609.5** (v1.9.2609 is upstream's current retail SM 6.9 compiler; SM 6.10 is preview-only at v1.10.2605.24). `include/native/directx` is `misyltoad/mingw-directx-headers` **pinned at its 2022-08-22 main**, so raising the header level to Agility 1.620.1 (retail 2026-10-28, `D3D12SDKVersion` 620) means either porting Microsoft's upstream headers to mingw again or replacing that fork: the interface surface it would add is `ID3D12GraphicsCommandList12` (batched async commands), fence barriers tier 1, DX dump files and the 1.620 shader-cache session APIs |
| LLVM 15 converter | Unchanged | major-version change requires AIR compatibility evidence |

## Validation

| Requirement | Status |
| --- | --- |
| Native + PE32 full D3D9 suite | Native PE64 and PE32 **37/37, 0 skipped** on `build64-2311`/`build32-2311` (`docs/status-index.md`, section 3.1). The single-artifact PE64 suite against `19a387b4…` finished **690 passed, 54 failed, 5 skipped**, exit **1**, **53 known / 6 new** (`.agent-obj/logs/native64-final.log`). Its real `d3d9_extra` vs_2_0 fog regression is fixed in `7402dc37…` (16/16, `.agent-obj/logs/d3d9-extra-vs2.log`); the other five new fingerprints are timeout/count drift under the full suite. Wine source `stateblock` **14,738/0 failures**; `device` 11 environmental failures (159,119 checks); `d3d9ex` 2 environmental failures (1975 checks, crash-free); `visual` **918 real** of 920 summary (140,804 checks, 56 todo, 63 skipped, 2 todo-successes, 4 Q8 non-PSO, fog 0, filling convention 0) in `.agent-obj/logs/visual-poswhite.log` on `7402dc37…`; not identical to `visual-packed3.log`, and the difference is explained: `visual.c:6717` skips unless `D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES` is advertised, so `08dcd33` turned that skip into a run (+146 executed checks) and this artifact leaves 278 fewer failures (1198 → 920) — `docs/status-index.md`, section 3.3. PE32 full suite not run since `build32-2311` was built |
| External conformance strict gate | Implemented in `tests/conformance.py`: any selected non-`passed` verdict fails even when it matches the baseline; `--update` does not bypass it |
| External conformance execution | Partial: all four Wine D3D9 entries complete without crash; `stateblock` green; `device` 11 (all display-mode/window-rect environment) and `d3d9ex` 2 (the same window-rect limit) remain; `visual` **918 real** of 920 summary (filling convention 176→**0** on 2026-11-07, a half-pixel fix: `to_d3d11_viewport`; projected 64→50 after StretchRect took texture levels and the texture-transform flags stopped being whitelisted; signed 220→4, all four Q8W8V8U8 alpha-boundary values with no PSO error; fog 0). Full PE64 suite 690/54/5, exit 1, 53 known / 6 new; `d3d9_extra` fixed in `7402dc37…`, five new host-sensitive fingerprints remain. Prior guarded runs: d3d11 `test_blend` 38/38, d3d10core `test_create_buffer` 117/117; dxgi `test_create_swapchain` and d3d12 `test_create_device` timeout remain open; dxgi_test.exe rebuild blocked by mingw/Wine header clash |
| #147 typed-view descriptors | `d3d12_view_kinds` DXIL and DXBC pass |
| #148 raytracing opt-out | `d3d12_raytracing_off` DXIL and DXBC pass |
| Controlled failure for fan fix | Red 9 wrong / green pass |
| External Wine/vkd3d conformance | Partial: Wine 8.17 source and source-built `d3d9_test.exe` exist locally; no source-built d3d10/11/dxgi/d3d12 executables and no vkd3d-proton checkout |
| Shader-converter verification, GPU readback | Readback used throughout; converter matrix open |
| Stress, lifetime, failure injection | Partial |
| Performance before/after vs DXMT/D3DMetal | Baseline only: 118-scene D3D12 probe run, all scenes ok, `buildfresh/perf/baseline.json`; no before/after and no DXMT/D3DMetal comparison yet |
| Games / older GPUs | Unavailable on this host; synthetic tests used |

## The 8-sample paired path on pre-Apple10 GPUs (13 November 2026)

Upstream's `001fdb5` ("rasterize 8 samples as 4 where the GPU has no 8") pairs D3D's samples two-to-one on Apple GPUs
before Apple10. It was recorded and its baselines were made on an Apple M5 Pro, which is Apple10: there the pairing
never engages, so the path it added is the path this host runs and upstream's host does not.

Merged into `integrate-upstream-201`, two results say the paired path is not right on this GPU:

* `d3d11_ps_writes` fails 10 checks, **every one of them at 8 samples** (`while: 8 samples`), and it passed twice on
  this host before the merge. It is in neither side's `known.txt`: upstream had no failure to record because upstream
  never ran the path.
* `d3d11_raster_samples` moves from the 8 708 wrong this branch recorded to 32 962, with the 8x and 4x read jobs both
  affected (`sample 0 ... = 1, want 15`).

Upstream's own words carry the boundary: "Apple GPUs before Apple10 store at most 4 samples", and the feature's aim is
that "the shaders see D3D's count, coverage and sample numbers". On this host they do not: a pixel shader that runs per
sample counts Metal's four where D3D named eight, and a coverage read of an 8-sample target reports one bit set where
fifteen are wanted. `d3d11_msaa8`, the test upstream added for the feature, passes here, so what is missing is the
shader-facing half, not the resource half.

Triage of that half, so a fix does not start from zero. Two separate symptoms, not one:

* **Pixel frequency** (`SV_Position` only) reports **0 runs where 1 is due**, at both "8 samples, render target" and
  "8 samples, UAVs alone". That is coverage, not the sample count: a pixel the standard pattern covers is treated as
  covered by no Metal sample, so `DiscardOrCovered`'s paired mask never selects it.
* **Sample frequency** (`SV_SampleIndex`, `dxbc_signature.cpp:587` through `AIRBuilder::D3DSampleIndex`) reports
  Metal's indices where D3D's are due: pixels want 5, 6, 7 and get 3, 3, 2. The doubled index is not reaching the
  varying, or the shader runs Metal's four times where D3D names eight.

What is verified correct, so a fixer can skip it: `air.PairedSamples` is assigned in `convert_dxbc_pixel_shader`
(`dxbc_converter.cpp:1117`) -- the only of the three `convert_dxbc_*` entry points that needs it, and the one a pixel
shader takes; the `ShaderVariantPixel` brace-init's position for `paired_samples` matches the struct's declaration
order in all three `d3d11_pipeline*.cpp` sites; `Desc.PairedSamples` is computed the same way upstream computes it;
`texture_->paired`, `RasterSampleCount`, `SM50_TEXTURE_MULTISAMPLE_METADATA_PAIRED` and the `air_builder` helpers are
line-for-line upstream's. The merge dropped none of it. Also note the DXIL path never sets `PairedSamples` at all
(`dxil_converter.cpp` has no assignment on either side), so D3D12's paired path is a separate, wider question that
this test does not reach.

**Why the sample-frequency half cannot be fixed by remapping.** The counts the test reads are shader invocations:
`run()` does one `InterlockedAdd` per invocation, and the expectation for a pixel is the number of the standard 8
samples D3D11.3 19.2.4 places inside it. On this GPU the resource holds 4, so Metal invokes the shader 4 times at
most; doubling `SV_SampleIndex` and widening the coverage mask changes what each invocation *sees*, not how many of
them *run*. A pixel wanting 8 invocations cannot reach 8 while the pipeline has 4 samples, which is why the failures
are 3-against-5 and 2-against-5 rather than an off-by-one. The pixel-frequency failures are the same cause seen
differently: the paired coverage mask selects no Metal sample for a pixel D3D calls covered, so the pixel runs 0
times where 1 is due.

So this is a limit of the paired approach on a GPU without 8x, not a wiring bug in the merge, and the two ways out
are different products, not different patches: either sample-frequency shaders at 8 samples are emulated by invoking
the pixel function twice per Metal sample -- which changes what `InterlockedAdd`, `IncrementCounter` and any other
per-invocation side effect do, and is exactly what this test measures -- or the device stops claiming 8x for
sample-frequency work on families below Apple10, which is the option this branch had already documented in
`d3d11_raster_state` before upstream's change. Upstream's baselines were made on an Apple10 GPU, where neither
question arises. **This needs the owner's decision, not a guess at 3 a.m.**

This is a defect in the merged feature, not in the merge: the C++ and header plumbing was checked line by line against
upstream and is identical (`Desc.PairedSamples`, `texture_->paired`, `variant.paired_samples`,
`SM50_TEXTURE_MULTISAMPLE_METADATA_PAIRED`, `air_builder`). A pure-upstream build on this host was attempted as the
control and does not compile here at all -- `supportsPlacementSparse` returns `id` under this SDK at
`winemetal_unix.c:4190` -- so upstream alone cannot be measured on this machine.

Three failures upstream did close, measured passing on this host after the merge and removed from `known.txt`:
`dxgi_resize_queues`, `d3d11_resolve`, and `d3d11_raster_state` -- the last being the forced sample count of 8 this
branch had documented as unobservable without Windows. Upstream settled it by rasterizing 8 as 4. The `9/x86_64`
masked-family lines stay until that family is measured again.

## `d3d12_mesh_groups_dxil` hangs on both families -- GPU work that never retires (13 November 2026)

Not in either baseline; measured on the merged tree (`--family 9 --exact d3d12_mesh_groups_dxil`, 612 s, identity
46f9b069294d026f):

```
stuck: 1 in d3d12core.dll: mullion::MTLD3D12CommandQueueImpl<true>::CommitHeld()
       1 in d3d12core.dll: mullion::MTLD3D12CommandQueueImpl<true>::Watch()
       1 in winemetal.dll: SharedEventListener_start()
       1 in d3d12_mesh_groups.exe: execute(...)
       1 in winemetal.dll: MTL4CommandBuffer_waitForGPU()
```

Established by the stack, not inferred:
* The application thread is blocked in `waitForGPU` -- it is **waiting for GPU work that does not retire**, not
  spinning on the CPU.
* `CommitHeld` and `Watch` are both alive and waiting normally, so this is not the queue's held-work or watchdog
  thread deadlocking against the app.
* **It hangs identically under family native and family 9**, both alone and in the full sweeps
  (`17b4f947f2bd9974` native, `8522926870e4eb1f` family 9) with the same five threads. An earlier revision of this
  section said native passed and only the masked family hung. That was **wrong** -- it was inferred from the sweep's
  report of a *new* fingerprint under family 9 instead of read from the log, where native shows the same 612 s
  `still running` line. So nothing points at a family gate; the stall is not masking-specific.
* The trace narrows it further: the stalled buffer is **command buffer 1**, the first the queue commits, and under
  both families the heavy trace says the same thing -- `status 2 (committed), 3 (scheduled)`, never completing, with
  `encoders by kind: 2 0 1 2 0 0 0 0 0 0 0 0 0` (2 Null, 1 Render, 2 Blit; `EncoderType` order in
  `d3d12_command_encoder.hpp:27-43`). The same command buffer number, the same encoder mix and the same status under
  both families: the work is scheduled and does not retire, and it is the very first submission of the process.

Ruled out by reading the code: the grid-splitting loop in `DispatchMesh` (`d3d12_command_list.cpp:5285-5306`) cannot
spin -- each of `k += slices`, `j += rows`, `i += width` advances by at least 1 (`rows`/`slices` are floored at 1),
and the degenerate zero-count and zero-limit cases return early at line 5289 before the loop.

Not determined: the cause. What is now known is where to look -- a Metal 4 command buffer, the queue's first, that
reaches `scheduled` and never completes, on a GPU where the native family is what the test was written against. The
mesh gates found (`support_indirect_command_buffers`, `d3d12_pipeline_graphics.cpp:1274-1277`;
`maxTotalThreadgroupsPerMeshGrid`, line 2037; `d3d12_command_allocator.cpp:530`'s note that mesh commands in indirect
command buffers come only with Apple9) are no longer the prime suspects, because the stall is identical with and
without the mask.

Until then the entry is recorded in the ledger rather than added to `known.txt`, because it has no cause yet, and a
known-failure line without a cause is exactly what `docs/TESTING.md` 9.1 forbids.

## `d3d12_profile_dxbc` under family 9: a command buffer stamped after the run closed (13 November 2026)

Appeared in the family-9 sweep of the merged tree and in **neither** baseline, so it is a new failure rather than a
renumbered one:

```
wrong: command buffer CPU clocks outside run (while: child MULLION_DIAG=profile, run 4)
```

What is established by measurement:
* It reproduces under family 9 (`--repeat 3`), and **the offending run index moves** (3 in one sweep, 4 in another).
  So it is not "run 3 is broken": it is whichever command buffer is last.
* It is family-9 only. The native sweep does not report it.
* The record's own fields are what the check bounds: `diag::Line("cmdbuf", ..., buffer.committed, completed, ...)`
  (`mullion_command_queue.cpp:373`), and `d3d12_profile.cpp:544` asserts `began <= row[3] && row[4] <= ended`.

What is inferred, and deliberately not written as fact: `completed` is taken on the completion thread, not the
recording thread -- so the last buffer's completion can legitimately land after the child stopped taking records,
and family 9's slower path makes that likelier. That explains the moving index better than any logic error does. It
is not proven: the record file the child writes is not retained after the run, so which of the two fields breaks --
`committed` before the window, or `completed` after it -- has not been observed, only reasoned about. Getting that
would mean changing the test to keep its records, which is a change to make when fixing it, not to satisfy
curiosity now.

Why it matters beyond one test: if `completed` is genuinely after the window, the check itself is asserting something
the record cannot promise, and the fix belongs in the contract (bound `committed`, allow `completed`) rather than in
racing the completion thread. If instead `committed` precedes the window, the stamp is wrong and the library needs
the fix. **The two outcomes are different patches, and this section does not know which one it is yet.**

## `dxgi_factory`'s hang: Wine never emits the minimize WinEvents (13 November 2026)

`dxgi_factory` was recorded as "still running" with no cause. Answering the tearing query correctly (commit 963b4d2)
moved the test forward instead of clearing it, and where it now stops identifies the defect.

The step registers an occlusion-status event, minimizes and restores the window, and waits `INFINITE` on that event.
`StatusNotifications::add` (`src/dxgi/dxgi_factory.cpp:56-124`) arms `SetWinEventHook` over
`EVENT_SYSTEM_MINIMIZESTART..END` for exactly this. **Wine registers the hook and never emits either event**: no
non-test source in `dlls/win32u`, `dlls/user32` or `dlls/wined3d` generates them -- only
`dlls/user32/tests/msg.c` names them, tagged `winevent_hook_todo`. Under Wine the notification therefore has no
source and the wait can be satisfied by nothing. The hook is not wrong; it is what Windows needs, and it stays.

A fix was attempted through the state Present already computes, implemented, and **disproved by the test itself** --
so it was reverted and nothing of it is in the tree. What was learned is worth more than the patch would have been:

* The plumbing turned out to be easy after all: `MTLD3D11SwapChain` already holds `Com<IDXGIFactory1> factory_`
  (`d3d11_swapchain.cpp:1156`), contrary to what this section said when it was written. The notification crossed as a
  call on a new `IMTLOcclusionNotifier` façade (a second COM base on the factory does not compile -- it inherits
  `AddRef`/`Release` twice from different `IUnknown`s), and the whole thing built and ran.
* **It did not fix the hang, and cannot.** The notify came from `PresentFrame`. A title that minimizes and then
  waits on its registered event never presents again, so the one case the contract is about is exactly the case
  that produces no notification. Measured after wiring it end to end: `dxgi_factory` still stops at the same step
  (`f1ca0b14104f1664`), unchanged.
* Therefore the source cannot be the presenter. It has to be something that observes the window independently:
  Wine's own message path (the factory watching `WM_SIZE`/window-state for a registered window), or a watcher that
  re-checks `wsi::isMinimized` for the registered windows while listeners exist. The hook stays for Windows, where
  it does fire.

What is *also* worth recording, because an earlier revision of this section called the next thing a defect and was
wrong: `MTLD3D12SwapChain::Present` never calls `wsi::isMinimized` (`d3d12_swapchain.cpp:875-878` decides on zero
width/height only) -- and that is **correct**. D3D12 chains are restricted to the flip effects
(`dxgi_factory.cpp:240` refuses anything that is not `FLIP_SEQUENTIAL` or `FLIP_DISCARD`), while D3D11 gates the
minimize case on `desc_.SwapEffect <= DXGI_SWAP_EFFECT_SEQUENTIAL` (values 0 and 1) precisely because "you will not
receive DXGI_STATUS_OCCLUDED if you're using a flip model swap chain" (IDXGISwapChain::Present). A D3D12 chain can
never satisfy that gate, so nothing is missing; its two `DXGI_STATUS_OCCLUDED` sites are a zero-size case and a
fullscreen-fallback case, and both are as specified. `DXGI_STATUS_OCCLUDED` from Present and the registered-event
notification remain separate questions either way.

## The zero-failure campaign (7 November 2026)

The full PE64 suite on `ac85c8e` was 692 passed, 52 failed, 5 skipped; on the 7 November evening tree it is
**706 passed, 38 failed, 5 skipped** of 749 (`.agent-obj/logs/final2-pe64-full.log`, run with `toolchains/llvm-mingw-20261104-ucrt-macos-universal/bin` first on `PATH` so a hung test can be symbolized). The 52 failures fall into eight
root-cause clusters; fixing a cluster fixes every failure in it. The clusters, with each one's evidence:

| Cluster | Failures | Root cause, as the logs show it |
| --- | --- | --- |
| BC4_SNORM handling | 4 (down from 10; `block_values` ×2 where **the load path decodes exactly** — the spec's integer arithmetic, precise division — and 150 point-sample checks remain where the point-sample's companion reads as zero while the load's is
    correct — a texture-identity question the gated `MULLION_BC_DEBUG` kernel diagnostic answers next session;
    `copy_formats` ×2 at one unit) | **the architecture is unified with the D3D11 layer's**: a BC4/BC5 SNORM resource stores the raw blocks (copies, locks, footprints and CPU maps are all native now), a SNORM view reads the raw storage's RSnorm view, and `d3d11_block_values` passes outright through the companion. What remains for D3D12's exactness: the companion (`bc_snorm`, the member both APIs share) filled by an inversion kernel — fetch the raw texture's unorm texels, recover the endpoint bytes exactly, invert each texel's index, decode per the spec — recorded as a compute pass after every copy into the resource, since D3D12 writes BC resources only by copying. Metal's own RSnorm decode is the spec value through a byte promotion (`b×256/32767` vs `b/127`) whose endpoints also skip the spec's clamp, so a scale factor alone cannot be exact. `copy_formats`' one unit is a raw reinterpret copy (RGBA16F texels into a BC4 SNORM footprint) whose unit mapping follows the source texture, not the footprint |
| Rasterization edge rules | 8 (`raster`, `raster_bias`, `raster_samples`, `d3d10_raster`) | post-snap degeneracy and tie-breaking differ from D3D's rules at pixels where two edges meet; the bias failures are narrowed to **planar depth-stencil attachments only** — the same bias cases on the non-planar D32_FLOAT pass, so the planar pass's depth handling, not the bias math, is where the depth stays at its clear value. The general fix is vertex pre-snapping in the converter (D3D11.3 15.16's 1/256 grid), which also needs an AIRCONV_VERSION bump and a full-suite re-run |
| MSAA quality counts | 0 (was 6 — **fixed this pass**) | Direct3D 12's own format tables make 8x optional at every format, and this GPU answers no 8-sample texture; the tests require 4x and hold 8x to consistency |
| Format capability answers | 0 (was 2 — **fixed this pass**) | a decoded BC SNORM format's capability is now what a block-compressed format may do, not what its float storage's would |
| Tiled resources | 6 (`tiled`, `tiled3d`, `sparse_remap`, `sample_residency`) | tile contents after remap differ in whole tiles; a residency readback reports a resampled mip as resident where the reference has it not |
| Wave operations | 4 (`wave_types`, `wave_helpers`) | the wave mask a pixel shader reports disagrees with the lanes its own leader/lane fields say took part: the SIMD group's ballot or match carries lanes the reconstruction excludes (or lacks), which needs a focused reproduction of the quad-to-lane mapping helper lanes produce; the exclusion machinery exists (`simd_is_helper_thread` guard, `over_lanes` ballot) and the failing pixels are the diagonal corners of two quads in one group |
| Metal API misuse | 2 (`format_cpu`) | an AGX assertion `bytes_per_row >= used_bytes_per_row` fires on a small-width copy |
| DXGI window/queue | 5 (`resize_queues`, `factory`, `composition`, `queued_present`) | a swapped queue's readiness lags one frame; a tearing feature-size query errors; D3D10 composition returns E_NOTIMPL; a queued Present timed out once (host-sensitive) |
| Reference-blocked, not a defect to guess at | 1 (`d3d11_raster_state`) | A forced sample count of 8 on a GPU that rasterizes none: the documented value domain says 8 is valid, and no page says what a device must do when its hardware cannot. This build's answers already agree with each other (`CreateRasterizerState2` and `CheckMultisampleQualityLevels` both gate on `supportsTextureSampleCount`, d3d11_state_object.cpp:640-643 and d3d11_device.cpp:1182-1191), which is what CODE.md 2.2 requires. Settled only by an observation on Windows; the expectation is not moved to match the build and the build is not moved to match the test |
| Host and misc | 5 (`capture`, `features`, `gather`, `residency`, host_mutate/format) | a capture's trace group is named differently; 4 of 98 format capability answers differ; one comparison-gather channel; residency progress; two host-side script bugs |

The 32-bit-only defects found this pass: the same depth-bias emulation as the raster_bias cluster
(`d3d11_raster_bias`, `d3d11_xrbias` fail on PE32 while passing on PE64), and the DXGI window/queue trio.

## `d3d12_residency_dxbc`: the DXBC spin path is 100-300x slower above occupancy -- and the work does complete
### (13 November 2026; this section was rewritten twice, and what the rewrite retracts is part of the record)

### The measurement

One binary, a positional argument choosing the compiler (`dxbc`, `dxil`). Run bare it takes the DXIL default, which is
why a first reproduction appeared to show nothing. At identical bounds (`TURNS=16777216 MS=5000`), same host:

| groups | DXIL | DXBC | items done (both) |
|---|---|---|---|
| 64 | 1.52 ms | 1.77 ms | 2000 / 2000 |
| 256 | 7.66 ms | **814.70 ms** | 2000 / 2000 |
| 448 | 2.46 ms | 516.70 ms | 2000 / 2000 |
| 1440 | 2.57 ms | 252.54 ms | 2000 / 2000 |
| 4096 | 7.32 ms | 668.13 ms | 2000 / 2000 |

Every group count, both compilers, all 2000 items, and places claimed is exactly groups + 2000 in both columns. With
its default bounds the DXBC run reports "failed: 8 wrong results" and the DXIL run passes.

### What this is, and what it is not

**It is not a correctness defect, and an earlier version of this section said it was.** The claim there was that "a
`globallycoherent` DXBC load does not see another group's write", that the kernel "stops early" and "leaves items
unfinished", and -- worst -- that this was "game-breaking", that a title distributing work between threadgroups "does
not complete its work". All of that is **retracted**: with a turn bound large enough for the path, the DXBC kernel
finishes every item and its claim arithmetic matches DXIL exactly. Writes are observed. Nothing is lost.

What the earlier reading mistook for lost work is the test's own `TURNS` bound firing. That bound is not an arbitrary
timeout: the contract says it exists so that "a kernel in trouble" stops in a few seconds instead of forever. Here it
did what it is for -- it noticed that this path needs one to two orders of magnitude more spin rounds than the other.

**What remains real is the performance gap, and it has a shape.** At 64 groups the two compilers are within 0.25 ms
of each other. The divergence starts when the dispatch exceeds what the device keeps resident: 100-300x more time per
hand-over above occupancy, and it does not grow with the group count (252 ms at 1440, 668 ms at 4096, 815 ms at 256),
so it is not a queueing artifact of having more groups.

That is the case the contract predicts -- "a GPU that keeps only some of the groups running side by side and takes
turns among the rest makes every hand-over to a group that is not running wait a whole round, which is seconds here"
-- so the honest statement is that DXBC pays that cost far more expensively than DXIL on the same pattern, not that it
fails to pay it.

### What was tested and ruled out

* `SM50_SHADER_FLAG_COHERENT_GROUP_SYNC` (`MULLION_SHADER_FLAGS=32`): does not fix the bound failure, though it moves
  the numbers (at 1024 groups, items done 492 to 975, claims 1516 to 1999). A fence at the barrier is not the gap.
* `MULLION_AIRCONV_SKIP=hoist-loads`, and `MULLION_AIRCONV_HOIST_LOADS=0`: 524, 497 and 500 items done at 256 groups --
  indistinguishable from the default 322 ms/~500. The load-hoisting pass is not moving the spin loop out of its loop.
* `MULLION_AIRCONV_SKIP=simdgroup-barrier`: same, 508 items. Not the implicit-memory-barrier pass either.
* Progress is *not* capped: `TURNS` 262144, 1048576 and 4194304 give 489, 1870 and 2000 items done. That measurement
  is the one that overturned the visibility story, and it was available the whole time.

**The AIR says the same thing as the TURNS measurement, and closes the visibility question at codegen level.** There
*is* an airconv CLI, already built: `build64-2311/src/airconv/darwin/airconv`, a native Mach-O binary from
`src/airconv/airconv_cli.cpp` via `src/airconv/darwin/meson.build:39`. An earlier revision of this section said the
meson build had no such target. It does -- the Windows-side `executable('airconv', ...)` at
`src/airconv/meson.build:107` is gated on `build_airconv_for_windows` (default off, and enabling it fails here because
it wants `toolchains/llvm/include`, which does not exist), and the darwin one is the working form. Toggling that option
in `build64-2311` was tried and reverted; the build directory configures and builds clean again, verified.

Dumping the kernel with `MULLION_TRACE_SHADER=heavy MULLION_SHADER_DUMP_ALL=1` and running it through
`airconv -S` gives 324 lines of AIR for the residency compute shader (identifiable by its `262143` turn bound):

* The spin loop's read of the released slot is **`air.atomic.global.load.i32`** (`%115` at line 226, `%133` at 246) --
  an atomic device load, which the optimizer may not hoist out of the loop or serve from cache. There is no stale-load
  bug to find here, which is the codegen counterpart of the fact that the items all finish.
* The claims are `air.atomic.global.add.u.i32` with `memory_type 0` (device) and `memory_order 2`, and the releases are
  `air.atomic.global.store.i32` at the same scope and order.
* The barrier is **`air.wg.barrier(i32 2, i32 1)`** -- device memory flag, no separate `CreateAtomicFence` -- so
  `SupportsNonExecutionBarrier()` was false for this compile and the barrier carries the ordering itself
  (`nt/dxbc_converter_base.hpp:267-276`).

So what remains to explain is the cost per turn, not the correctness of the data flow. The shape worth measuring is
whether a device-scope atomic load and a device-scoped barrier per spin iteration is what Metal executes as an explicit
memory barrier every time, because that would cost roughly what is seen. Getting that answer needs the MSL or GPU
assembly for this kernel, and `airconv` writes AIR (`-S`) or a metallib (`-A`), not MSL.

### Whether the test's bounds should change is a separate question, and it is not answered here

`TURNS=1<<18` and `MS=250` are calibrated so the DXIL variant passes. Left alone, DXBC reports a failure every run.
Raising them would make the number look clean and would weaken the check that caught a 100-300x pathology, which is
the specific thing this repo's rules forbid doing to a failing test. So the bounds stay as they are, the defect stays
open in this section, and the choice is the owner's -- with the note that the *right* end state is probably that the
DXBC path stops needing larger bounds, not that the bounds get larger.

### It cannot be put in `known.txt` either

Three single runs gave `failed: 8 wrong results` at three digests ([8b10fadd], [ab1c95c3], [c7c65bbc]) and `--repeat 3`
reports `unstable:`, which `run.py` refuses to load from the file by design: the digest covers the failure text, and
this test's text carries measured milliseconds and item counts, which move every run. A line was installed and removed
for exactly that reason. So it reports as new on every sweep until either the fingerprint excludes measured quantities
or the test separates its timing claim from its correctness claim -- raised, not taken.


## where `d3d11_fullscreen` actually stops (13 November 2026)

The ledger carried this as an unexamined failure. A native run to the log's last line (5358af6a32f6f531) locates it,
and it is **not in Mullion's code**:

```
step: seed=1 Direct3D=10 output=0 switch=1 created=0 target=0 visible=1 mode=960x600@59000/1000
info: Setting display mode: 960x600@59
stuck: 1 in d3d11.dll: mullion::wsi::setWindowMode(HMONITOR__*, HWND__*, mullion::wsi::WsiMode const&)
       2 in d3d11.dll: std::__1::__contention_wait<8ull, ...>
```

The path holds no lock of ours. `setWindowMode` (`wsi_window_win32.cpp:109`) queries the monitor, builds a `DEVMODEW`,
and calls `setMonitorDisplayMode`, which returns early when the mode already matches (line 62) and otherwise calls
**`::ChangeDisplaySettingsExW(monInfo.szDevice, pMode, nullptr, CDS_FULLSCREEN, nullptr)`** (line 66), retrying
without `DM_DISPLAYFREQUENCY` if that is refused (line 72). Reaching the `info:` line proves the early return did not
fire, so the thread is inside Wine's call. The two `__contention_wait` frames belong to other threads parked in the
C++ runtime, not to this path.

So the test blocks in Wine's display-mode change on this host -- in a prefix where ordinary window creation is fine,
since `windows.exe` and `window_sync.exe` pass. What Wine's driver does with a `CDS_FULLSCREEN` request for a mode the
desktop does not offer is the open question, and it is a Wine-side or environment question rather than a translation
defect. Changing our call would be a guess, and the refresh-rate retry at line 72 is already the accommodation the
code makes for a refusal.

Recorded as *located* rather than *closed*: knowing where it stops is worth having, and it is a different thing from
knowing why.
