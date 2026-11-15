# Status index — every governance claim, and where it lives

This index answers one question for every document the commit history names: **where is it, and is it there?**

The campaign plan (`docs/MULLION_REQUIREMENTS.md`, `OWNER-REPORT.md`) and the commits that cite them refer to three
governance artifacts by description rather than path — a *requirements matrix*, a *zero-failure campaign ledger*, and
an *owner report*. Until each resolves to a file, an "Open" requirement can disappear by being written in a document
nobody can find. It does not have to be reconstructed: all three are in the tree, and this table is the mapping.

## 1. Resolution table

| Claimed by | Named as | Path | Added by | Status |
| --- | --- | --- | --- | --- |
| `8def50d`, `be5234f`, `f0013a8`, `e94de38`, `09cde96`, `f4ceb98` | "the requirements matrix" | `docs/MULLION_REQUIREMENTS.md` | `be5234f` (2026-11-06) | **present** |
| `f0013a8`, `e94de38`, `ab354c8`, `ebdf1d6`, `b620f30` | "the zero-failure campaign ledger" | `docs/MULLION_REQUIREMENTS.md`, section "The zero-failure campaign (7 November 2026)" (line 84) | `f0013a8` (2026-11-06), extended by `e94de38` … `b620f30` | **present** |
| `09cde96`, `f4ceb98`, `bfb40ea`, `ab354c8`, `ebdf1d6`, `b620f30` | "the owner report" | `OWNER-REPORT.md` (repository root) | `09cde96` (2026-11-07) | **present** |
| `be5234f` | "game-tests, advertised-feature and parity updates" | `docs/game-tests.md`, `docs/game-tests/*.jpg`, `docs/advertised-features.md`, `docs/game-parity.md` | `be5234f` | **present** |
| `eb4d9cc` | "the details in the wiki" (README's split) | `docs/ARCHITECTURE.md`, `docs/CODE.md`, `docs/DEBUGGING.md`, `docs/TESTING.md` | `eb4d9cc` (2026-10-30) | **present** |
| `33fe0f6` | tracing | `docs/tracing.md` | `33fe0f6` (2026-10-27) | **present** |
| `f48f199` | advertised game features / parity | `docs/advertised-features.md`, `docs/game-parity.md` | `f48f199` (2026-10-29) | **present** |
| `c538e06` | DXMT's notices | `docs/licenses/dxmt.txt`, `docs/licenses/dxmt-mit.txt` | `c538e06` (2026-10-30) | **present** |
| `a9c59fe` | the reviewed drafts | `CONTRIBUTING.md`, `docs/CODE.md` | `a9c59fe` | **present** |
| `06c2d96` | the implementation report, the PR description | `MULLION_IMPLEMENTATION_REPORT.md`, `PR-DESCRIPTION.md` | `06c2d96` (2026-11-07) | **present** |

**Unresolved claims: 0.** Nothing was added and then removed: `git log --diff-filter=A --name-only -- docs
.agent-obj OWNER-REPORT.md MULLION_IMPLEMENTATION_REPORT.md PR-DESCRIPTION.md` shows one add per governance file, and
`git log --diff-filter=D --name-only -- docs` removes only five screenshots (in `eb4d9cc`, together with the README
lines that showed them), never a document.

### How the enumeration was made

```bash
# every governance file the history added, by the commit that added it
git log --diff-filter=A --name-only --pretty=format:'@@%h|%ad|%s' --date=short -- \
    docs OWNER-REPORT.md MULLION_IMPLEMENTATION_REPORT.md PR-DESCRIPTION.md PERF-NUMBERS.md
# every commit whose subject claims a document
git log --oneline --first-parent --grep='^docs:' d3d9-mission-completion      # 14 on this branch, 18 on all refs
# nothing governance-shaped was deleted
git log --diff-filter=D --name-only -- docs OWNER-REPORT.md
```

Fourteen `docs:`-prefixed commits on `d3d9-mission-completion`; the documents they cite are listed in the table
above. `docs/game-tests.md` cross-references the requirements matrix and the campaign ledger; both are the same file,
which is why the two names look like two documents.

## 2. Reference audit — the paths the documents themselves name

Every backticked path with a file extension in `README.md`, `CONTRIBUTING.md`, `OWNER-REPORT.md`,
`MULLION_IMPLEMENTATION_REPORT.md`, `PR-DESCRIPTION.md`, `PERF-NUMBERS.md`, `docs/*.md`, `src/d3d9/README` and
`.agent-obj/CHECKPOINT-2026-11-06.md` was resolved against the working tree (by exact path, else by basename
anywhere outside `.git` and `build*`):

| | Count |
| --- | --- |
| distinct path references | 161 |
| resolve to a file in the tree | 150 |
| do not resolve | 11 |

The 11 that do not resolve are names a run produces or a game owns, not evidence:

| Reference | Cited by | Why it is not a file in the tree |
| --- | --- | --- |
| `/tmp/gate_pe64_archknown.log`, `/tmp/gate_pe32_archknown.log` | `docs/game-parity.md` | a `/tmp` scratch log from a past pass; not preserved |
| `Game_d3d11.log`, `Game_d3d12.log` | `docs/DEBUGGING.md` | the log a game writes beside its executable |
| `dxmt.conf` | `docs/DEBUGGING.md` | another product's configuration file |
| `mullion.log`, `mullion.prev.log` | `docs/tracing.md` | where `MULLION_TRACE_*` writes at run time (`~/Library/Logs/Mullion/…`) |
| `CombatMaster.exe`, `amd_ags_x64.dll`, `rendersystemdx11.dll`, `swarm/cfg/video.txt` | `docs/game-tests.md` | files inside the game installs the parity runs used |

The two `/tmp` gate logs are the only *evidence* names that are gone. They are replaced evidence, not lost evidence:
the same two gates are `.agent-obj/logs/gate-pe64-full.log` and `.agent-obj/logs/gate-pe32-d3d12-final.log`.

Every `docs/*.jpg` is absent from `docs/` — `eb4d9cc` removed five screenshots together with the README lines that
showed them — so no document links a picture it does not have. The images that survive in the tree
(`docs/game-tests/*.jpg`) are the ones `docs/game-tests.md` names.

## 3. What the plan's four contradictions resolve to, with today's artifacts

The completion plan was written on 2026-11-06 against a tree that did not yet have the requirements matrix or the owner
report. Four of its six contradiction findings have since changed shape. Each verdict below names the log and the
artifact, which is the contract the plan's T0.2 exists to enforce and which `tests/run.py` now emits on every run
(`tests/manifest.py` → `meson-logs/runs/<identity>/attempt-N/manifest.json`, one immutable directory per
attempt).

### 3.1 D3D9 "37 declared, 36 gated" — **closed**

`tests/d3d9/meson.build` declares 37 tests. `build64-2311` and `build32-2311` declare **37** and each ran **37 passed,
0 failed, 0 skipped** today:

| Gate | Log | Manifest | Artifact |
| --- | --- | --- | --- |
| PE64 D3D9 | `.agent-obj/logs/gate-pe64-d3d9-r3.log` | `f124fe747b9bc838` | `build64-2311`, `d3d9.dll` `269a818d145bf5e6`, llvm-mingw 20261104, wine-8.17 |
| PE32 D3D9 | `.agent-obj/logs/gate-pe32-d3d9-r3.log` | `ac68af060f6b500c` | `build32-2311`, `d3d9.dll` `e9d304fe77194c37`, llvm-mingw 20261104, wine-8.17 |
| PE64 D3D9, `--validate` | `.agent-obj/logs/gate-pe64-d3d9-validated.log` | `e01079eece5fb36f` | same, Metal API + GPU validation enabled |
| PE32 D3D9, `--validate` | `.agent-obj/logs/gate-pe32-d3d9-validated.log` | `8706c90e17871523` | same |

`d3d9_position_colour` (added by `38355e6`) is the 37th test, and it is in all four runs: this is the first gate that
contains it. The earlier 36-test gates (`gate-pe64-d3d9.log`, `gate-pe32-d3d9.log`,
`toolchain-2311-*-d3d9.log`, `pe32-d3d9-halfpixel.log`) predate that commit and are not evidence about it.

### 3.2 `706/38/5` vs `692/52/5` vs `690/54/5` — **one artifact each, and 749 is not a total**

| Split | Log | When | Build | Known entries | Arithmetic |
| --- | --- | --- | --- | --- | --- |
| 706 / 38 / 5 | `final2-pe64-full.log` | 11-07 18:43 | `build64-2311` (ls 20261104) | 36 | 749 = 750 − `d3d9_position_colour` |
| 692 / 52 / 5 | `gate-pe64-full.log` | 11-07 10:20 | `buildfresh` (ls 20260908) | 53 | 749 = 749 declared |
| 690 / 54 / 5 | `native64-final.log` | 11-05 22:53 | `buildfresh` | 53 | 749 |
| 690 / 54 / 5 | `native64-final2.log` | 11-05 23:44 | `buildfresh` | 54 | 749 |

The plan recorded that no log prints 706/38/5. `final2-pe64-full.log` does, and it is the newest of the four. The
three splits are three artifacts and three `tests/known.txt` revisions, in time order, not one baseline moving. The
"all three total 749" coincidence is explained: `build64-2311` now declares **750** tests, and the one it added after
`final2-pe64-full.log` was produced is `d3d9_position_colour` — the same test as 3.1. A suite total is therefore only
meaningful with the build directory it was read from.

### 3.3 `920` vs `1198` — the plan's resolution is **inverted**: 920 is the newer, better artifact

| Log | When | Executed | Todo | Failures | Real | Todo-succeeded |
| --- | --- | --- | --- | --- | --- | --- |
| `visual-packed3.log` / `visual-vs2.log` | 11-05 22:16 / 22:56 | 140 658 | 62 | 1198 | 1194 | 4 |
| `visual-poswhite.log` | 11-08 09:46 | 140 804 | 56 | 920 | **918** | 2 |
| `visual-mova12-stdout.txt` | 11-14 20:5x | 140 804 | 43 | 349 | **331** | 11 |
| `visual-cnd1-stdout.txt` | **11-14 21:4x** | 140 804 | 43 | 321 | **310** | 11 |

The executed-set difference is one skip becoming a run, not a test list changing. The 11-14 run is the translator's
shader-model-1 relative addressing: `take_source` read vs_1_x's inline `c[a0.x + regnum]` as if the address register
came in the token that follows (the vs_2_0/ps_3_0 form), which desynced every SM1 instruction after a relative source
(`test_mova`'s draw answered D3DERR_INVALIDCALL), and a shader model 1 `mov a0.x` now floors as Windows does (the
translator's other paths are untouched; `test_mova` went 18 → 0, both ABI d3d9 gates stay 88/0 ×3, and Wine's
`device.c` count is unchanged). The run's logs carry `MULLION_LOG_PATH` in `.agent-obj/logs/visual-mova12/`.

Two more translator faults, the same evening, found with an extended `tests/d3d9/d3d9_shader_cnd.cpp` (the coissue
forms it did not cover) and bisected against the airconv CLI's own `-S` dump of the translated program:
- the coissue shortcut of `cnd` did not name the shader model: ps_1_4's +cnd compares every component like a plain
  cnd does (Wine's own comment), so the shortcut now gates on `minor < 4`;
- the implicit `[0, 1]` clamp of every ps_1_1-1_3 arithmetic write was wrong: only the explicit `_sat` destination
  modifier clamps (wined3d's GLSL backend emits clamp() for `_SAT` alone, and Wine's cnd_test comment builds 2.0 in
  a register with "adding c0.r(=1.0) to c0.r into r1.r" and passes it to a dp3, which the clamp halved and the
  comparison then missed). Wine visual.c: cnd_test 10 → 0, the run 321 → 310 failures; `device.c` count unchanged;
  both ABI d3d9 gates 88/0 ×3 (identities `c0d0b1a582c51d5e` PE64, `eea56aa124ca49e0` PE32). `d3d9_test.exe` is the same binary
in both (`c011468d95e953ef…`, 2026-11-03). `packed3` contains

```
Tests skipped: Blitting from textures is not supported.
```

which is `visual.c:6717` — `if (!(caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) skip(…)`. Commit
`08dcd33` advertised that cap, so the whole `test_stretchrect_from_textures` now executes (+146 checks) and
`poswhite` skips `D3DFMT_A1R5G5B5 support is not D3DOK_NOAUTOGEN` instead. Both runs skip 63 checks; the sets differ
by exactly those two lines.

Failure clusters, by enclosing test function, `packed3` → `poswhite` (`918` + 2):

| packed3 | poswhite | Function | Plan task |
| ---: | ---: | --- | --- |
| 168 | **0** | `test_filling_convention` | T1.3 (fixed by `ca05ddd`) |
| 81 | **51** | `projected_textures_test` | T1.1 |
| 62 | 62 | `stream_test` | T1.6 |
| 49 | 49 | `test_multisample_stretch_rect` | T1.8 |
| 48 | **24** | `test_viewport` (`visual.c:14725`) | T1.2 — **not settled** |
| 44 | 44 | `cnd_test` | T1.10 |
| 35 | **11** | `stretchrect_test` | T1.7 |
| 32 | 29 | `clear_test` | — |
| 30 | 30 | `volume_dxtn_test` | T1.4 |
| 24 | 24 | `test_specular_lighting` | T1.5 |
| 24 | 24 | `zenable_test` | T1.6 |
| 23 | 23 | `add_dirty_rect_test` | — |
| 21 | 21 | `test_vshader_input` | T1.11 |
| 21 | 21 | `shadow_test` | T1.6 |
| 20 | 20 | `test_vertex_blending` | T1.11 |
| 19 | **3** | `fp_special_test` | — |
| 19 | 19 | `test_color_vertex` | — |
| 18 | 18 | `test_mova` | T2.1 |
| 18 | 18 | `pretransformed_varying_test` | T1.11 |
| 18 | 18 | `multiple_rendertargets_test` | T1.6 |
| 17 | 17 | `depth_buffer2_test` | T1.6 |
| 19 | 16 | `texture_transform_flags_test` | — |
| 16 | 16 | `test_updatetexture` | T1.9 |
| 15 | 15 | `depth_blit_test` | — |
| 14 | 14 | `multisampled_depth_buffer_test` | T1.8 |
| 14 | 14 | `test_uninitialized_varyings` | T1.11 |
| 13 | 12 | `texdepth_test` | — |
| 8 | **0** | `clip_planes` | T2.4 |
| 5 | **0** | `np2_stretch_rect_test` | T1.7 |
| 0 | **4** | `test_mipmap_autogen` | new — regressed, T1.9 |
| 1 | 2 | `test_max_index16` | — |
| 1 | 0 | `fixed_function_decl_test` | T2.3 |

Every other function is unchanged, so 920 − 918 is accounted for line by line: 278 fewer failures, three clusters
gone (`filling_convention`, `clip_planes`, `np2_stretch_rect_test`), five reduced, one that grew
(`test_mipmap_autogen`, 0 → 4).

**Correction to the documents:** `OWNER-REPORT.md` and `docs/MULLION_REQUIREMENTS.md` both say `visual-poswhite.log`
is "916 real". The log says 918 `Test failed` lines plus 2 `Test succeeded inside todo block`, and the run's own
summary line says 920 failures. 918 is the real count.

### 3.4 "0 known" gates — the arithmetic was right; the comparison was not fail-closed

The D3D9 gates that printed `0 known` had nothing to compare: nothing failed, so `0` was correct. The defect the plan
found is real one level down — `known()` returned an empty list for a **missing** file, so `--validate` printed
`0 known, each with its cause in …/known-validated.txt` against a file that did not exist. Fixed:

- `tests/run.py::listed_by()` decides the list **before any test runs** and exits non-zero when it is missing, naming
  the list and why it is needed. `--record` is the one mode that runs without one, because it writes one.
- `tests/known-validated.txt` now exists, made from the two reviewed validated runs above. Both recorded no failure
  and no skip, so it holds none — the D3D9 gate under Metal validation is green on both ABIs, and that is now a
  statement about a file rather than about an absence.

### 3.5 Fingerprint drift — `unstable` is a failure class, not a baseline edit

`tests/run.py` already runs each test `--repeat` times and reports a test that answers differently as
`failed: unstable: <one signature> / <another>`. What was missing is the guard that a drifting signature cannot be
*written down* as known. `known()` now refuses a line whose verdict is `unstable:` and says why: a test whose answer
moves between runs of one artifact has no identity to be known by. `tests/known.txt`'s reader only accepts
`failed:`, `skipped:`, `still running` and `exit status` verdicts, so the laundering path the plan names is closed at
the file format.

**Measured, 2026-11-08 — `d3d12_residency_dxbc`.** `--repeat 3` on one artifact (`build64-2311`, manifest
`0306c5ab20e633d4`) answered three different signatures in three runs:

```
unstable: failed: 8 wrong results [6f2452de] / failed: 8 wrong results [fac6e41d] / failed: 8 wrong results [53bbb67f]
```

The cause is in the test's own output, not in the runner's digest. Two of the eight `wrong:` lines are wall-clock
measurements:

```
wrong: the dispatch took 344.5 ms, more than 250 ms (while: 2000 items handed down a line by 256 groups …)
wrong: 269 of 2000 items were done, and the kernel says it is not finished
```

A result that carries a duration has no fingerprint to pin, so the `native(Apple M4 Pro)/x86_64` entry that told a
reader to "refingerprint after a full run's log" has been withdrawn from `tests/known.txt` (a comment there records
why), and the test is reported as `unstable` — a failure — until T3.8 separates its timing budget from its correctness
result. The `9/x86_64` entry for the same test is left in place: it has not been measured this way yet.

Not yet run: `--repeat 3` on `dxgi_resize_queues`, `d3d12_residency`, `dxgi_factory`, `d3d11_fullscreen`,
`d3d12_capture`. `dxgi_resize_queues` (128 → 150 → 162 → 170) and `d3d12_residency` are the same class as the above
until shown otherwise.

### 3.6 Viewport — still open, and the 24 that remain are one line

`poswhite` has 24 failures at `visual.c:14725` and **0** at `14742`; `packed3` had 24 + 24. Both lines are
`check_rect(&rb, tests[i].expected_rect, tests[i].message)` inside `test_viewport`, on two different case tables. The
plan's rule stands: this cannot be called a test-side defect without a Windows D3D9 observation, and it cannot be
fixed in the library without one either. T1.2 remains `blocked — evidence missing` (the 3-way probe at
`(MinZ, MaxZ)` = `(0, 0.001)`, `(0, 0.5)`, `(0.25, 0.75)`).

## 4. Builds under test (T0.3)

| Build | Machine (`llvm-readobj`) | `buildtype` | `b_ndebug` | d3d9 / d3d12 / tests | Compiler |
| --- | --- | --- | --- | --- | --- |
| `build64-2311` | `IMAGE_FILE_MACHINE_AMD64` (0x8664) | release | `false` | on / on / on | llvm-mingw 20261104 (clang 23.1.5) |
| `build32-2311` | `IMAGE_FILE_MACHINE_I386` (0x14C) | release | `false` | on / on / on | llvm-mingw 20261104 |
| `buildfresh`, `build32` | same, older toolchain (20260908) | release | `false` | on / on / on | llvm-mingw 20260908 |

`b_ndebug=false` on all four, so `assert` is live in the code under test.

**ARM64EC/X: blocked — missing prerequisite.** The cross compiler is present (`arm64ec-w64-mingw32-{gcc,g++,ar,strip,
windres}`, clang 23.1.5, target `arm64ec-w64-windows-gnu`) and `build-arm64ec.txt` declares `-marm64x`, but configuration
stops at `src/winemetal/meson.build:29`:

```
ERROR: C shared or static library 'winecrt0' not found
```

because `meson.build` takes the import libraries from `$(wine_install_path)/lib/wine/aarch64-windows` for
`cpu_family == 'aarch64'`, and `toolchains/wine/lib/wine/` holds only `i386-windows` and `x86_64-windows` — that Wine
has no arm64 support at all. The prerequisite is an arm64 Wine build (its `aarch64-windows` import libraries and
`winebuild`), not a flag: T5.4 is unstarted, not passed, and no ARM64X result may be claimed until one exists.
`.github/check-arm64x.py` has never run here either. The half-configured `build-arm64ec` directory was removed (it
produced no `build.ninja`).

## 5. What is still not evidence

- A current PE32 full run exists: `.agent-obj/logs/m0-pe32-full.log`, 489 passed / 247 failed / 4 skipped, identity
  `39c1e663bef0c3b9`, on the pre-D3D9-fix artifact. Its 740 declared tests all answered. The old 284/238/4 number was a
  smaller tree and is not comparable.
- A current PE64 full run exists: `.agent-obj/logs/m0-pe64-full.log`, 709 passed / 37 failed / 5 skipped, identity
  `19edee6d0abb2231`, also pre-D3D9-fix. The D3D9 gates after the fixes are current: PE64 `ede7ada443667c06`, PE32
  `5eb56f8de4da9763`, validated `be6b5d21154f1637` / `1c711344ba0032d5`, each 37/37.
- The four Wine D3D9 entries now have current PE64 runs: visual 901 failures of 140804 executed (was 920), device 70,
  stateblock 0, d3d9ex 53; see `.agent-obj/logs/wine-d3d9-*-current.log` and the execution checkpoint.
- The snapshot now exists and was restore-verified; the two drift entries in `known.txt` were re-measured with
  `--repeat 3` and describe their real causes (DXGI tearing query, `SharedEventListener_start`).
- `visual-poswhite.log` and `visual-*.log` are untracked; they are preserved in `.agent-obj/logs/`, and were
  captured by the fresh snapshot `.agent-obj/snapshots/mission-b620f3023217f5aee9dd719edf00ee6d7e4e418a-20261108-162314`
  (`tools/snapshot.sh`, verified by restoring tracked and untracked files into a temporary directory); the
  post-execution snapshot `…-20261108-191256` also carries the runner attempts. The older
  `.agent-obj/snapshots/buffy-20261106-mission.tgz` predates them.
- Conformance (`T4.x`), ARM64EC (`T5.4`), MetalFX/interpolation (`T7.1`) and performance (`T7.5`) have no result here.
