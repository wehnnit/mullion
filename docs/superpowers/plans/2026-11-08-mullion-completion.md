# Mullion Remaining-Work Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute this plan task-by-task. Use systematic debugging for failures and evidence-based verification before claiming completion. Checkpoints preserve continuity; they do not end the assignment.

**Goal:** Finish the remaining D3D9/9Ex, D3D10/10.1/11/12-to-Metal implementation, architecture support, compatible component integration, conformance, robustness and measured performance work, then prepare a local owner-permission request.

**Architecture:** Preserve the current D3D9-on-D3D11 frontend and existing shared Mullion, airconv and winemetal infrastructure. Backport justified improvements selectively. Fix behavior in its owning layer and cover every affected frontend, shader path and ABI. Preserve the existing PE32 D3D12 descriptor-table implementation while completing its rendering paths.

**Tech stack:** C++/C, Objective-C Metal bridge, Python test infrastructure, Meson/Ninja, llvm-mingw, LLVM/AIR conversion, Wine/WoW64, DXBC/DXIL, Metal 3/4 and MetalFX.

**Planning date:** 2026-11-08. Historical counts below identify evidence; execution must refresh them against the source and artifacts actually under test.

## 1. Global constraints and definition of completion

- Preserve all existing tracked and untracked work, dependency pins and evidence. Do not reset, clean or discard unrelated work.
- Use recoverable snapshots and isolated Wine installations/prefixes. Protect personal CrossOver applications and bottles.
- Do not commit, upload, publish or contact anyone externally during implementation. The final action in this plan is preparing a local permission request and waiting for the owner's explicit authorization.
- Follow actual tool permissions. Missing public dependencies are acquisition/build tasks; a permission request must identify the particular operation that needs approval.
- Do not guess at a fix, fabricate execution, weaken specifications, silently skip required cases or turn a failure into a pass by changing a baseline.
- Keep PE32 D3D12, ARM64EC/ARM64X and mixed-bitness sharing in scope. An API-refusal switch is not implementation of that API.
- Preserve valid failure semantics, COM ownership, GPU lifetime, ordering, bounds and cleanup. Never report success after dropping application work.
- Every capability claim must agree with resource creation and actual execution. Accurate reporting of an unavailable optional capability does not complete a separately required feature.
- Unavailable games do not block synthetic, conformance, setup or implementation work. Finite tests cannot establish that every game works; make only evidence-supported compatibility and performance claims.
- A vendor/runtime prerequisite must remain visibly unverified while other available work continues. Do not relabel it as completed.
- This document is an execution plan, not evidence that its unchecked tasks have run.

**Completion means:** every required contract has an implementation, applicable architecture coverage, distinguishing tests, exact artifact provenance and an accepted result. A report update, completed run, stable failure fingerprint or local green subset does not satisfy this definition by itself.

## 2. Starting evidence and corrections to preserve

Read before execution:

- `CONTRIBUTING.md`, `docs/CODE.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`.
- `docs/MULLION_REQUIREMENTS.md`, `docs/status-index.md`, `OWNER-REPORT.md`.
- `.agent-obj/CHECKPOINT-2026-11-08-execution.md` and its referenced logs/manifests.
- `docs/d3d9-architecture-decision.md`.
- Any applicable repository instructions discovered at execution time.

| Checkpoint | Verified saved result | Qualification |
| --- | --- | --- |
| Local D3D9 PE64 | 37 passed / 0 failed / 0 skipped | Identity `ede7ada443667c06` |
| Local D3D9 PE32 with native bridge | 37 / 0 / 0 | Identity `5eb56f8de4da9763` |
| PE64 D3D9 with Metal validation | 37 / 0 / 0 | Identity `be6b5d21154f1637` |
| PE32 D3D9 with Metal validation | 37 / 0 / 0 | Identity `1c711344ba0032d5` |
| Full PE64 | 709 passed / 37 failed / 5 skipped | Precedes the latest two D3D9 fixes |
| Full PE32 | 489 passed / 247 failed / 4 skipped | Precedes the latest two D3D9 fixes |
| Wine D3D9 visual | 899 ordinary failures + 2 unexpected todo successes = 901 reported | 140,804 checks; 56 expected todos; 63 skips |
| Wine device | 11 ordinary failures + 59 unexpected todo successes = 70 reported | 159,087 checks; 28 skips |
| Wine stateblock | 14,738 checks; zero failures/skips | Preserve this regression coverage |
| Wine d3d9ex | 2 ordinary failures + 51 unexpected todo successes = 53 reported | 1,975 checks; 45 skips |

The 247 PE32 failures are **whole-suite failures**, not all D3D12: 232 D3D12, eight D3D11, three D3D10 and four DXGI cases. Recount after subsequent changes.

Already implemented; preserve rather than repeat:

- Content-based untracked-source identity, loader-probe evidence, immutable attempt directories and required-manifest checks.
- Position-only white colour packing, fixed indexed fans and their distinguishing tests.
- Public vertex sampler IDs 257–260 mapped to compact internal storage, independent of pixel samplers.
- Four MRTs and independently stored per-target colour-write masks.
- Getter-independent additional-chain presentation, implicit-object zero-count coupling and substantial explicit-resource lifetime work.
- FVF/declaration caching, private-data fixes, state-block restoration and actual GDI/cursor/SYSTEMMEM paths.
- Eight-stage fixed-function infrastructure, with remaining texture-type and vertex-path combinations to finish.
- Existing PE32 D3D12 handles, CPU-visible allocation changes and WoW64 bridge.
- Typed-descriptor compatibility, raytracing opt-out and existing typeless-view fixes.
- The two latest D3D9 fixes: non-autogen filter acceptance and retained depth-stencil state across target changes.

## 3. Execution loop and evidence record

Before starting a task, identify its exact failing input, contract and affected consumers. For each behavioral change:

1. Reproduce the failure on an identified artifact.
2. Write or strengthen a registered regression with a specification/reference-derived expected result.
3. Observe a distinguishing failure on the faulty implementation or a controlled broken variant.
4. Implement the correction in the owning layer, including error and cleanup paths.
5. Run the regression on PE64 and PE32 when those consumers are reached.
6. Run the originating external group and affected shared-API tests.
7. Preserve all attempts, including failed attempts, and update the ledger.
8. Select the next available unfinished action and continue.

Every task record must contain: task ID, API contract/reference, labeled hypothesis, affected files/consumers, source identity, artifact/loaded-module identity, exact command, expected/actual verdict, architecture/capability configuration, evidence path and next action.

Use implementation states `Open`, `Investigating`, `Implemented` and `Verified`. Track unavailable external verification separately; `Implemented` is not `Verified`. A measured hang is still a failing task.

Session limits require an exact resumable checkpoint: current edits, commands, installed artifacts, incomplete processes, latest results and the next concrete action. Do not declare the assignment complete to close a session.

## 4. Dependency order

```text
Recovery and evidence integrity
             |
Current complete baselines
             |
   D3D9 rendering ------- Shared API defects ------- PE32 D3D12
          |                       |                       |
     D3D9 breadth          Modernization/integration      |
          +-----------------------+-----------------------+
                                  |
                  Final conformance and robustness
                                  |
                  Controlled performance acceptance
                                  |
                    Local owner-permission request
```

Acquire ARM64 Wine, build external suites, prepare header ports and develop the performance harness alongside available debugging. Integrate shared-layer changes in reviewed batches so failures remain attributable. Do not wait for every D3D9 task to finish before acquiring other prerequisites.

## Task 0 — Preserve recovery and evidence integrity

**Files:** `tools/snapshot.sh`, `tests/manifest.py`, `tests/manifest_probe.cpp`, `tests/host/manifest.py`, `tests/run.py`, `tests/mullion-test`.

**Consumes:** current dirty tree, dependency pins, existing immutable attempts. **Produces:** recoverable source/evidence snapshot and trustworthy run identities.

- [ ] Reuse the completed infrastructure; verify a fresh snapshot includes current tracked/staged changes, untracked source, pins and ignored runner evidence that must be retained.
- [ ] Restore representative tracked, untracked and evidence files into a temporary location and compare their bytes. Do not modify the working checkout to test restoration.
- [ ] Preserve the manifest host regressions and required-manifest behavior.
- [ ] Verify manifests distinguish source contents, selected files and loader-probe results; preserve WoW64 `system32` redirection handling.
- [ ] Corroborate actual test-process module loading for cases with different overrides/loading behavior; do not assume a separate probe covers every per-test environment.
- [ ] Verify executable architecture, compiler/SDK identity, bridge identity and immutable attempt paths before an acceptance run.
- [ ] Keep actual verdicts and baseline differences independently visible. A matching failure baseline is not success.

**Accept when:** restore works, artifact mismatch checks fail closed, and no run needed for acceptance can be overwritten or attributed to an unidentified installation.

## Task 1 — Run the post-fix complete baselines

**Files:** existing Meson builds, `tests/run.py`, `tests/conformance.py`; evidence under immutable attempt directories.

- [ ] Rebuild `build64-2311` and `build32-2311` with assertions enabled and verify actual build options.
- [ ] Run complete PE64 and PE32 local suites on the resulting artifacts, including the native bridge for WoW64.
- [ ] Repeat local D3D9 gates with and without Metal validation.
- [ ] Run all four Wine D3D9 entries against the current PE64 artifact; build/run the corresponding PE32 conformance executables as part of Task 12.
- [ ] Reconcile declared selections with actual results; missing required tests, missing compilers and incomplete runs must not pass.
- [ ] Record coverage changes when capabilities cause previously skipped groups to execute.

Existing runner command shapes, to execute only during implementation:

```bash
meson compile -C build64-2311
meson compile -C build32-2311

python3 tests/run.py --build build64-2311 \
  --wine toolchains/wine --prefix toolchains/tests-prefix \
  --family native --require-manifest d3d9

python3 tests/run.py --build build32-2311 --build build64-2311 \
  --wine toolchains/wine --prefix toolchains/tests-prefix32 \
  --family native --require-manifest d3d9
```

Repeat with `--validate`; omit the selector for full suites. Confirm the build-selection logic installs the correct PE modules and native bridge. Use the actual isolated prefix paths recorded by the run.

**Accept when:** every declared current test has an actual verdict or explicit missing-coverage reason, and every current failure maps to a task below. This task inventories failures; it does not accept them.

## Task 2 — Finish fixed-function cube/volume sampling first

**Files:** `src/d3d9/d3d9_fixed_function.hpp`, `src/d3d9/d3d9_fixed_function.cpp`, generated fixed-function shader headers, `src/d3d9/d3d9_device_draw.cpp`, `tests/d3d9/fixed_function_shaders.cpp`, `tests/d3d9/d3d9_mips.cpp`, cube/volume tests and their Meson registration.

**Consumes:** existing eight-stage path and the selective-backport architecture decision. **Produces:** dimension-correct shader variants and selection for 2D, cube and volume textures.

- [ ] Reduce the three remaining `test_mipmap_autogen` cube-colour checks into a local regression with distinguishable face colours and mip values.
- [ ] Record each stage's actual texture dimension, coordinate source and required coordinate defaults.
- [ ] Generate reproducible shader variants and select dimension-correct resources/sampling instructions. Do not bind a cube resource to an unconditional `Texture2D` declaration.
- [ ] Cover mixed dimensions across stages, texture-type changes between draws, null textures and cache-key changes.
- [ ] Cover programmable/fixed-function combinations, blended vertices, pretransformed vertices and existing eight-stage paths.
- [ ] Run PE64/PE32 regressions, the full local D3D9 suites and the originating Wine visual entry.

**Accept when:** all three known checks pass for the correct sampling reason and broader cube/volume/stage switching tests pass without loss of existing coverage.

## Task 3 — Finish projected coordinates and vertex input

**Files:** `d3d9_fixed_function.cpp`, `d3d9_device_draw.cpp`, `d3d9_device_shader.cpp`, `d3d9_vertex_declaration.cpp`, `d3d9_shader_translator.cpp`; corresponding files under `tests/d3d9/`.

- [ ] Reproduce every projected-texture failure with declaration component counts, transform counts, projection flags and shader profile recorded.
- [ ] Build a reference matrix for one-to-four component inputs, transformed/untransformed coordinates and fixed-function/programmed vertex paths. Establish the divisor; do not treat an unverified declaration-component rule as fact.
- [ ] Fix perspective division, coordinate defaults, register mapping and cache/state invalidation at their owning layer.
- [ ] Resolve stream/offset/stride/frequency, instancing, vertex input, position-index, texcoord-index and pretransformed-varying failures.
- [ ] Verify FVF/declaration transitions, UP/stream/indexed draws, nonzero offsets and large indices on both ABIs.
- [ ] Evaluate existing `ProcessVertices` against its full output/declaration contract before selectively porting another implementation.

**Accept when:** all mapped projection and vertex-input groups pass with distinguishing per-coordinate/per-stream data and no regressions in the current fan, position-colour or sampler tests.

## Task 4 — Complete shader-model 1–3 semantics

**Files:** `src/d3d9/d3d9_shader_translator.cpp/.hpp`, `d3d9_device_shader.cpp`, `src/airconv/`, shader tests and external Wine fixtures.

- [x] Inventory every valid opcode/register/profile combination promised by advertised shader models, plus invalid bytecode rejection.
- [x] Resolve CND, MOVA, TEXBEM, texture operations, relative addressing, flow control, modifiers, precision and varying initialization groups.
- [x] Separate vertex and pixel register/sampler limits from fixed-function capability fields.
- [x] Verify all public vertex samplers 257–260, pixel samplers through s15 and independent state-block restoration.
- [x] Verify converter error propagation, LLVM verification, shader variant identity and cache invalidation for semantic changes.
- [x] Add boundary inputs that distinguish correct math, signed decoding, NaNs/zeros and address rounding from accidental passing output.

**Accept when:** each advertised shader profile's required operations and malformed-input behavior have coverage on both architectures; no valid draw succeeds while silently omitting its shader work.

## Task 5 — Finish fixed-function lighting, combiners and geometry

**Files:** `d3d9_fixed_function.cpp/.hpp`, `d3d9_state.cpp/.hpp`, `d3d9_device_draw.cpp`, generated shader sources and local fixed-function tests.

- [ ] Finish all eight combiner stages across supported vertex paths, including alpha operations, argument modifiers, coordinate generation and transforms.
- [ ] Resolve specular, colour-vertex, material-source, lighting, normal handling and vertex-blending groups.
- [ ] Complete point size, sprites, scaling and semantic point fill; test triangle culling/clipping and repeated/shared vertices rather than equating point fill with arbitrary POINTLIST submission.
- [ ] Preserve flat/smooth shading, clipping planes, transformed input and declaration/FVF behavior.
- [ ] Exercise adjacent draws with different states to detect stale variants/constants and state-block omissions.

**Accept when:** every advertised fixed-function capability is demonstrated across the relevant input paths, not merely accepted by a setter.

## Task 6 — Finish viewport, depth, stencil and MRT behavior

**Files:** `d3d9_common.hpp`, `d3d9_device_targets.cpp`, `d3d9_device_draw.cpp`, `d3d9_state.cpp`, `tests/d3d9/d3d9_targets.cpp`, write-mask and depth tests.

- [ ] Investigate zero/reversed viewport depth ranges using the pinned Wine compatibility rule `max_z = max(max_z, min_z + 0.001f)` and available Windows/reference observations. Do not dismiss the existing tests as impossible.
- [ ] Test ordinary/degenerate ranges with XYZ/XYZRHW, fixed/programmed vertices, LESS/GREATER/EQUAL and nearby depth clears. Keep any D3D9-specific adjustment out of unrelated API paths.
- [ ] Verify retained depth-stencil bindings across target changes, both binding orders, failed calls and restoration.
- [ ] Specifically test a legally larger depth surface with a smaller render target. The current `bound_depth_stencil()` omits views unless dimensions match exactly; retaining the pointer alone does not prove correct rendering for that contract.
- [ ] Resolve z-enable, shadow, depth-buffer, multisampled-depth, clear and depth-blit groups.
- [ ] Preserve sparse MRT slots, independent per-target write masks, full RGBA sentinels, blend interaction and state-block behavior.
- [ ] Verify failed clear calls have no partial colour/depth side effects.

**Accept when:** depth/stencil and MRT results agree with their contracts across sizes, formats, sample counts and call orders, with matching getters and actual GPU output.

## Task 7 — Finish resources, compression, transfers and mip behavior

**Files:** `d3d9_texture.cpp/.hpp`, `d3d9_surface.cpp/.hpp`, `d3d9_device_texture.cpp`, `d3d9_device_transfer.cpp`, shared format/copy code and resource tests.

- [ ] Resolve dirty-rectangle, UpdateTexture, mip upload, maximum-mip and autogen groups after the sampler-type correction.
- [ ] Complete compressed volume behavior with raw API-visible block storage and a decoded companion or equivalent justified emulation when Metal lacks the native representation.
- [ ] Cover subregions, row/slice pitches, mip chains, cube faces, volume slices, locks, copies and invalidation after every writer.
- [ ] Resolve signed-format/Q8 alpha boundaries and maintain the passing BC SNORM regressions.
- [ ] Audit L8 companion creation/failure behavior: an unsuccessful luminance conversion must not silently return incorrect red-only sampling.
- [ ] Finish advertised pool/usage/shared-handle combinations, CPU/GPU synchronization and managed-resource lifecycle.
- [ ] Verify GetDC/ReleaseDC contents, pitch/orientation, repeated acquisition and failure cleanup; preserve existing implementations.
- [ ] Complete StretchRect, filtering, resolves, compressed and multisample format restrictions, clipping and preservation outside the affected region.

**Accept when:** every mapped format/transfer/mip failure is resolved and the resource's API-visible bytes and sampled GPU contents remain coherent after all supported writers.

## Task 8 — Finish device lifecycle, concurrency and presentation

**Files:** `d3d9_device.cpp/.hpp`, `d3d9_swapchain.cpp/.hpp`, `d3d9_state_block.cpp`, `d3d9_query.cpp`, resource ownership classes, `src/dxgi/` and presenter/window code.

- [ ] Recheck implicit zero-count transitions, explicit resource/device references, texture-level shared counts, private bindings and in-flight GPU retention.
- [ ] Exercise state blocks, Reset/ResetEx, device-before-child release, reentrant destruction and multithreaded use without reference cycles.
- [ ] Preserve additional-chain real buffers and getter-independent presentation; test identities, retained references, intervals, failure and destruction.
- [ ] Finish query behavior, frame latency with observable scheduling effect, gamma, cursor and vertical-blank/presentation semantics.
- [ ] Reproduce remaining device/d3d9ex window failures with point/pixel dimensions, DPI, monitor modes and fullscreen transitions recorded. Fix the owning layer when possible; require a reduced reproduction before identifying a true host constraint.
- [ ] Remove arbitrary mode-enumeration truncation as a substitute for correct termination and complete enumeration.
- [ ] Complete Ex sharing, residency and lifecycle combinations.

**Accept when:** local and source-based lifecycle/presentation tests complete without hidden ownership failures, stalls or unexplained geometry mismatches.

## Task 9 — Complete interface and capability contracts

**Files:** `src/d3d9/d3d9.cpp`, `d3d9_direct3d.cpp`, `d3d9_device.hpp`, D3D9/D3D12 interoperability implementation and export/ABI tests.

- [ ] Audit every D3DCAPS9 field against the actual implementation and required shader-profile minima. Do not confuse fixed-function caps with programmable sampler counts.
- [ ] Exercise shader-validator vtable calls and behavior on PE32 and PE64, including malformed instructions and ownership.
- [ ] Implement genuine 9On12 device/queue/node interoperation, resource wrapping and synchronization; refusing supplied overrides is not completion.
- [ ] Verify exports, ordinals, COM identity, calling conventions and failure-output contracts on both architectures.
- [ ] Trace every Open/Partial D3D9 matrix row to Tasks 2–9 or a newly numbered concrete task; no unassigned long-tail groups may remain.

**Accept when:** supported interfaces and advertised capabilities match construction, failure and execution behavior, and every required D3D9 contract has evidence.

## Task 10 — Fix shared D3D10/11/12 and DXGI failures

**Files:** `src/d3d10/`, `src/d3d11/`, `src/d3d12/`, `src/dxgi/`, `src/mullion/`, `src/airconv/`, `src/winemetal/`; their registered local tests.

- [ ] Diagnose DXGI tearing-query behavior and the factory stall using exact inputs, stacks and last completed operations.
- [ ] Resolve fullscreen deadlock around `SharedEventListener_start` / `wsi::setWindowMode`; verify repeated transitions and destruction.
- [ ] Fix resize-queue readiness and queued presentation ordering.
- [ ] Resolve the capture-trace flake with repeated lifetime/trace tests.
- [ ] Trace DXBC residency progress separately from timing: 33–356 of 2,000 completed items is not explained by a variable duration string. Compare DXBC with passing DXIL and inspect barriers, atomics, dispatch and completion.
- [ ] Separate raster fill, sample locations, planar-depth bias and degeneracy cases. Treat global pre-snapping as an unproven hypothesis until distinguishing evidence supports it.
- [ ] Finish tiled-resource/residency and helper-lane/wave semantics, including capability-dependent fallbacks.
- [ ] Correct native small-copy row/block footprints and gather/format failures without dropping work or relaxing expected output.
- [ ] Map every refreshed full-suite failure to a concrete root-cause task and rerun every affected frontend.

**Accept when:** the complete current PE64 and applicable PE32 API suites pass their required contracts, not merely their known-baseline comparison.

## Task 11 — Finish PE32 D3D12 rendering and the bridge ABI

**Files:** `d3d12_descriptor_heap.hpp/.cpp`, `d3d12_heap.cpp`, resource/command/pipeline code, `winemetal.h`, `winemetal_thunks.c/.h`, `airconv_thunks.c/.h` and new registered ABI/readback tests as needed.

- [ ] Recount PE32 failures by API; isolate the first D3D12 divergence with paired PE64/PE32 workloads.
- [ ] Advance through upload/readback, compute, vertex fetch, constant pixel output, CBV/SRV/UAV/samplers, root tables, heap changes and indirect execution before advanced pipelines.
- [ ] Compare CPU-authored bytes, marshaled nested structures, descriptor contents, allocations, GPU addresses, command order and GPU output at each step.
- [ ] Preserve the ABI: PE32 CPU descriptor handles contain four-byte `SIZE_T`; GPU descriptor handles remain 64-bit.
- [ ] Audit pointer extension/truncation, `WMTMemoryPointer`, counts, nested arrays, offsets, alignment and calling conventions.
- [ ] Test descriptor-table capacity/exhaustion/reuse and allocation failures; preserve valid ownership through final GPU use.
- [ ] Verify CpuPlaced visibility, mapping addresses and lifetime across copy/dispatch/draw completion.
- [ ] Require a complete PE32 D3D12 gate and both shader frontends where applicable; do not accept an API-router refusal as parity.

**Accept when:** actual PE32 rendering and readback pass the complete required suite with the native bridge identified, and no 32-bit-only failure is hidden by baseline or bitness exemptions.

## Task 12 — Build and execute all required external suites

**Files:** `tests/conformance.py`, `tests/run.py`, `tests/mullion-test`, external-suite build support and pinned public dependencies.

- [ ] Preserve Wine 8.17 as the historical reference and acquire a pinned current Wine test revision for compatibility comparison.
- [ ] Build source-based D3D9/9Ex, DXGI, D3D10/10.1/11/12 suites for the required architectures; inspect existing executables before claiming they are absent.
- [ ] Acquire/build pinned vkd3d-proton and relevant vkd3d shader suites with matching compiler packages.
- [ ] Verify all START_TEST entries, testlist linkage, entry invocation, subprocess accounting and function selectors. Requested missing/empty suites must fail.
- [ ] Diagnose the D3D12 create-device timeout with module provenance, thread stacks and progress markers rather than simply increasing the timeout.
- [ ] Preserve ordinary failures, expected todos, unexpected todo successes, skips, crashes, timeouts and incomplete runs as separate categories.
- [ ] Treat required behavior inside a todo as remaining work; do not break a correct unexpected success to satisfy an old annotation.
- [ ] Keep actual-verdict acceptance separate from baseline changes and annotation maintenance. Baseline updates cannot bypass failures.

**Accept when:** every required case is accounted for, complete runs exist on the final artifacts, and failures or missing execution cannot be mistaken for a passing baseline.

## Task 13 — Obtain ARM64 prerequisites and verify ARM64EC/ARM64X

**Files:** `build-arm64ec.txt`, `src/winemetal/meson.build`, native LLVM/build configuration, isolated Wine installation and architecture-specific tests.

- [ ] Obtain/build a pinned ARM64 Wine with the required `winecrt0`, imports and build tools. Reuse verified repository CI pins where suitable.
- [ ] Provide a matching ARM64 native bridge and LLVM/AIR setup plus ARM64EC compiler/ARM64X configuration.
- [ ] Verify PE machine types, bridge architecture and actual loaded modules.
- [ ] Execute in a legitimate signed/entitled native Wine runtime; protect the original product installation when cloning one.
- [ ] Run the API contract suites and GPU readback on the architecture being claimed, including applicable x64 interoperability.
- [ ] Preserve exact prerequisite failures while continuing other available tasks. Build success alone is not runtime verification.

**Accept when:** every claimed architecture has real execution evidence. An unavailable signed runtime remains an explicit incomplete verification requirement.

## Task 14 — Verify mixed-bitness sharing and synchronization

**Files:** shared-resource/fence/keyed-mutex implementations under `src/mullion/`, `src/d3d11/`, `src/d3d12/`, `src/d3d9/`, bridge code and registered two-process tests.

- [ ] Run PE32 producer → PE64 consumer and the reverse with distinguishable resource contents.
- [ ] Cover handles, duplication, access/security attributes, close order, legacy/NT sharing where supported, keyed mutexes and fences.
- [ ] Test owner exit, abrupt death, abandonment, registry deletion/resurrection and waiting-process teardown.
- [ ] Verify shared structures contain portable handles/values rather than process-local pointers or pointer-width discriminators.
- [ ] Stress synchronization and lifetime under repeated process creation and concurrent access.

**Accept when:** both directions preserve bytes, ordering, access and lifetime without truncation, hangs or resurrection defects.

## Task 15 — Integrate compatible upstream changes

**Files:** affected shared/converter/bridge/API sources, associated upstream tests and cache/version definitions.

- [ ] Refresh upstream heads at execution time. The latest researched main was `78509d7ea5414b102a6a1e9cfde9fccb6490096c`; compare semantics, not only ancestry.
- [ ] Preserve existing typed-view, raytracing opt-out, typeless-UAV, shared-resource/fence/sampler and local D3D9 fixes.
- [ ] Integrate justified memory/descriptor storage, capture lifetime, pipeline sharing/archive, tessellation and mesh corrections in attributable batches.
- [ ] Verify command-list overlap preserves ordering between separate ExecuteCommandLists calls; verify pass joining preserves barriers, queries, attachment identity and load/store behavior.
- [ ] Evaluate resolve/gather/wave/sparse fixes against the actual GPU and OS. Do not apply blanket coordinate scaling from another machine's result.
- [ ] Evaluate asynchronous compilation, archive sharding, indirect optimizations and experimental pipeline changes with their own regressions. Compilation failures must not silently omit valid work.
- [ ] Do not import expectation-relaxing changes as a substitute for a required correctness fix.
- [ ] Reconcile converter/descriptor ABI changes and cache identity semantically; a larger version number alone is not proof.

**Accept when:** selected upstream behavior is integrated without losing local fixes, every affected test is registered, and there are no unexplained ABI/cache differences between components.

## Task 16 — Complete compatible components and modern DirectX behavior

**Files:** `include/native/directx`, compiler acquisition/build files, CI configuration, `src/airconv/`, API interfaces, winemetal and backend implementation.

- [ ] Verify current stable releases again before acquiring them. Research references: llvm-mingw 20261104, DXC v1.9.2609, DirectX-Headers v1.620.1 and Agility retail 1.620.1/SDK 620.
- [ ] Preserve the already-current DXC pin; align local/CI toolchain versions and checksums after testing.
- [ ] Port or replace the old MinGW DirectX header fork, checking COM layout, UUIDs, declarations and calling conventions on every ABI.
- [ ] Implement required modern interfaces and behavior through Mullion; installing Microsoft's Windows runtime is not a Metal backend implementation.
- [ ] Complete modern shader requirements: required 16/64-bit shader/wave operations, long vectors, special math, dispatch/view changes and DXR additions, with actual converted shader output verified.
- [ ] Complete remaining descriptor/root-signature, queue/fence/barrier, aliasing, residency, indirect, tessellation/GS/SO, mesh, raytracing, VRS, conservative raster, depth bounds, sampler-feedback, sparse/placed and writable-MSAA requirements.
- [ ] Cover D3D11On12, D3D9On12 and other required interface interoperation; audit optional/video interfaces for accurate advertised behavior.
- [ ] Separate stable retail targets from preview-only features. Do not advertise an unsupported feature level or tier to satisfy a checklist.
- [ ] Evaluate newer DXMT fixes against the credited base and preserve licenses/attribution.
- [ ] Keep native LLVM changes conditional on AIR/metallib compatibility, deployment support and full converter verification.

**Accept when:** current required interfaces, capability queries, creation and actual GPU execution agree across architectures; component versions and reproducibility metadata are consistent.

## Task 17 — Finish cache, pipeline and failure semantics

**Files:** shader caches, D3D12 pipeline libraries, persistent Metal archives, converter versioning and asynchronous compiler code.

- [ ] Test cold/warm startup, stale-version rejection, corrupt/truncated entries and concurrent processes.
- [ ] Verify every semantic input enters cache identity and pointer/padding bytes do not create accidental identity.
- [ ] Bound archive growth, use safe replacement and survive interrupted writes without poisoning later launches.
- [ ] Verify failed/cancelled compilation, device destruction and queued work preserve API failure and ownership semantics.
- [ ] Compare cold/warm output, not just cache-hit counts; old converted code must never survive an incompatible ABI change.

**Accept when:** cache and pipeline reuse preserve output and lifecycle across all tested conditions, including failure and process restart.

## Task 18 — Finish MetalFX and game-profile behavior

**Files:** `src/mullion/mullion_scaler.hpp/.cpp`, D3D11/D3D12 integration, `src/nvngx/`, relevant NVAPI/bridge/queue/presentation code and configuration/profile tests.

- [ ] Preserve current spatial/temporal and presented-scaler results.
- [ ] Complete motion-vector conversion, dynamic resolution, temporal-history reset, colour/depth/HUD handling and scaler recreation.
- [ ] Implement frame interpolation with explicit runtime/device availability, resource ownership, queue synchronization and pacing.
- [ ] Preserve correct real-frame rendering on systems where interpolation is unavailable.
- [ ] Verify NGX/NVAPI offers reflect implemented capability and actual runtime support.
- [ ] Finish profile selection, precedence, matching, overrides and diagnostic output. Per-generation refusal alone is not a game-profile implementation.
- [ ] Keep real-frame throughput, generated-frame output and input latency distinct in evidence.

**Accept when:** scaler/interpolation/profile behavior is demonstrated by pixels and timing on supported configurations, with correct fallback on older supported systems.

## Task 19 — Validate robustness and genuine platform differences

**Files:** affected API/bridge code, runner configuration and registered stress/failure tests.

- [ ] Run production Metal 4, Metal 4 API validation, applicable Metal 4 shader validation and Metal 3 shader-validation fallback configurations separately.
- [ ] Reduce suspected MetalTools/vendor failures to minimal reproductions with OS/GPU identity and stacks. Do not disable all validation to obtain a green report.
- [ ] Exercise real older GPUs when available; family masking proves branch coverage, not older hardware/compiler/scheduling behavior.
- [ ] Inject allocation/descriptor exhaustion, shader failure, device removal, queue destruction, pending waits and interrupted cache writes.
- [ ] Stress repeated create/destroy/reset, shared-owner death, concurrent bindings and reentrant cleanup.
- [ ] Verify wide counts/offsets before narrowing on PE32 and preserve ownership through final CPU/GPU use.
- [ ] Retain every attempt. A retry pass does not erase a crash, timeout or timing-dependent failure.

**Accept when:** required robustness cases pass; genuine external diagnostic/hardware prerequisites remain separately visible and prevent unsupported verification claims.

## Task 20 — Measure performance and optimize demonstrated bottlenecks

**Files:** existing performance probe, new registered D3D9/D3D11 scenes, `docs/game-parity.md` or the existing parity document selected by the repository, evidence manifests and measured hot paths.

- [ ] Extend the existing 118-scene, one-repeat D3D12 baseline with D3D9/D3D11 submission, state, shader, resource and presentation workloads.
- [ ] Add real engine/sample workloads using legitimately available software and record exact versions/settings/scenes.
- [ ] Run repeated paired before/after trials on identical hardware, source variants, output and cache conditions. Alternate execution order and record variability.
- [ ] Measure CPU submission, shader compilation, GPU time, median/p95/p99 frame times, stutters, memory and cold/warm launch behavior.
- [ ] Compare DXMT/D3DMetal only for APIs and workloads each backend actually supports; separate same-Wine backend comparisons from whole-product-stack comparisons.
- [ ] Keep validation off for timing but run separate validated correctness checks on the same implementation.
- [ ] Optimize only measured bottlenecks, then rerun output/regression checks and the repeated comparison.
- [ ] Count generated frames separately. Do not infer latency or compatibility from aggregate FPS.

**Accept when:** improvements are reproducible with unchanged correct output and honest spread/limitations. Do not claim arbitrary superiority or universal game compatibility.

## Task 21 — Final exact-source acceptance

**Files:** all final artifacts, complete test records, `docs/MULLION_REQUIREMENTS.md`, `docs/status-index.md`, `OWNER-REPORT.md`, applicable feature/parity documentation.

- [ ] Rebuild the exact final source with all selected dependencies and verify every loaded artifact.
- [ ] Run complete required PE64/PE32 local and external suites after the last relevant source change.
- [ ] Run applicable ARM64EC/ARM64X, mixed-bitness, validation, stress and cache gates.
- [ ] Account for every test selection, required skip, expected todo, unexpected success, crash, timeout and incomplete run.
- [ ] Resolve every actual required failure; do not remove failing known entries or required features just to make a file empty.
- [ ] Verify packaging layout for architecture-specific Wine directories/prefixes, exports, ordinals, licenses and reproducible dependency pins without altering personal installations.
- [ ] Reconcile every requirement with source, test and manifest evidence. Remove stale counts and unsupported claims.
- [ ] Keep unavailable external verification visibly incomplete. Do not call the entire assignment complete while a required gate is unverified.
- [ ] Run formatting/diff checks and preserve a final recoverable snapshot with all evidence.

**Accept when:** every required implementation and verification item is satisfied, the final report is internally consistent, and no available unfinished engineering action has been omitted.

## Task 22 — Write the local owner-permission request

**Create only after Task 21 passes:** `OWNER-PERMISSION-REQUEST.md`.

- [ ] Address the Mullion owner respectfully and explain the concrete problem solved and resulting behavior.
- [ ] Summarize completed API, architecture, compatibility and performance changes using measured evidence.
- [ ] Link `OWNER-REPORT.md`, the reconciled requirements matrix, exact source identity, manifests, complete result summaries and reproducible commands.
- [ ] Credit upstream authors and preserved license/attribution obligations.
- [ ] State material risks accurately. If a required task remains incomplete, return to execution instead of describing the contribution as finished.
- [ ] Ask for explicit permission to submit the completed contribution for review.
- [ ] State that nothing has been uploaded or published and wait for the owner's explicit authorization before any external submission.

**Suggested request opening:**

> I have completed the local Mullion changes described in the attached report and would like your permission to submit them for your review. The report identifies the exact source, architecture coverage, test evidence and measured performance. Nothing has been uploaded or published. Please let me know whether you authorize submission and whether you prefer a particular review structure.

This plan ends at preparing that local request. It does not authorize sending it or performing any external action.

## Final completion checklist

- [ ] All original requirements have task coverage; no Open/Partial implementation is hidden or abandoned.
- [ ] D3D9/9Ex rendering and full interface/capability contracts pass on PE64 and PE32.
- [ ] D3D10/10.1/11/12 and DXGI required suites pass on their required architectures.
- [ ] PE32 D3D12 rendering works, not merely device creation or refusal routing.
- [ ] ARM64EC/ARM64X and mixed-bitness claims have actual runtime evidence.
- [ ] Modern components, interfaces, converter output, bridge ABI and cache identity agree.
- [ ] Required external conformance, validation, stress and failure gates are complete.
- [ ] Performance conclusions are repeated, controlled and output-correct.
- [ ] The exact final source/artifacts, packaging, documentation and evidence are reconciled.
- [ ] The owner report and local permission request exist, with no external action taken.

## Reference sources to refresh during execution

- Mullion current-main reference: https://github.com/dttdrv/mullion/commit/78509d7ea5414b102a6a1e9cfde9fccb6490096c
- Microsoft Agility releases: https://devblogs.microsoft.com/directx/directx12agility/
- Shader Model 6.9 retail: https://devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more/
- DirectX-Headers: https://github.com/microsoft/DirectX-Headers/releases/tag/v1.620.1
- DXC: https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609
- llvm-mingw: https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104
- DXMT reference: https://github.com/3Shain/dxmt/commit/647b0020ba5764e5593b12407afd76964fa11e20
- Metal capabilities: https://developer.apple.com/metal/capabilities/
- MetalFX interpolation: https://developer.apple.com/documentation/metalfx/mtlfxframeinterpolator
- Apple game-porting tools: https://developer.apple.com/games/game-porting-toolkit/

**Next execution action:** verify the saved recovery point and final-source D3D9 artifacts, then implement Task 2's dimension-correct fixed-function cube/volume sampling while preparing the complete post-fix baseline. Continue through the ledger; do not stop after that subsystem.
