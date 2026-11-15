# Debugging

This document describes how to investigate a problem in Mullion, from the first failing run to a test that
reproduces it. What a bug report contains is in [CONTRIBUTING.md 1](../CONTRIBUTING.md#1-reporting-a-problem), and
how a fix is shown to work is in [TESTING.md](TESTING.md). Mullion's diagnostic tools are being extended, so the
parts that wait on that work are marked `{logging: ...}`, and those that wait on the test suite `{suite: ...}`. They
are filled in with each update from v0.5 on.

## 1. Establish the failing run

An investigation starts from one run that fails and can be repeated. Its record names the executable, the Mullion
revision, the Wine version, the configuration and settings, the steps that lead to the failure, and what was seen.

The kind of failure decides the evidence it needs:

- a game that does not start: the logs and Wine's console output ([section 2](#2-collect-and-read-the-logs))
- wrong rendering: a screenshot, the shader involved and, where possible, a capture
  ([section 5](#5-capture-a-metal-frame))
- a crash: the crash report with its stack, and the flight record ([section 7](#7-read-a-hang-or-a-slow-frame))
- a hang: the thread stacks of both sides of the bridge, taken while it hangs
  ([section 7](#7-read-a-hang-or-a-slow-frame))
- slow rendering: a profile of the frames ([section 7](#7-read-a-hang-or-a-slow-frame))

Evidence of one kind does not settle another. A shader compiler error does not show that the shader caused a visible
defect, and a thread waiting for the GPU does not show that the GPU is stuck.

A contributor without a Mac can still read supplied logs, follow the source locations they name, examine the shaders
involved and prepare a reduced case ([section 9](#9-reduce-the-problem-to-a-test)). A claim about how Metal executes
something stays unconfirmed until the affected path runs on a suitable Mac.

## 2. Collect and read the logs

Two environment variables control Mullion's logs ([`log.cpp`](../src/util/log/log.cpp)):

- `MULLION_LOG_LEVEL` is one of `trace`, `debug`, `info`, `warn`, `error` and `none`, in lowercase. Any other value,
  or the variable unset, selects `info`.
- `MULLION_LOG_PATH` names a directory, which must already exist, as a path the Windows process can open. Each DLL
  writes its own file there, named after the executable and the DLL, such as `Game_d3d11.log` and `Game_d3d12.log`.
  The value `none` turns files off. Without the variable, Mullion writes to Wine's console output only.

A file is created at the first message it accepts, and the next run of the same executable replaces it. Each attempt
therefore gets a directory of its own, and its logs and Wine's console output are kept before the next attempt
starts. If a file is missing, either nothing reached the level, output was turned off, or the directory could not be
written. A missing file alone does not show that Mullion was not loaded.

A refusal reported through `FAIL(code)` ([`log.hpp`](../src/util/log/log.hpp)) is logged as a warning with the
function, the error code and the source location, so the level `error` hides refusals. Wine's console output is kept
beside the files, since the native shader cache reports there and not in them. {logging: the values that decided a
refusal, and a time, process and thread on every line}.

## 3. Check the configuration

The options are listed in [`mullion.conf`](../mullion.conf) and read by
[`config.cpp`](../src/util/config/config.cpp) in this order:

1. the file `MULLION_CONFIG_FILE` names, or else `mullion.conf` in the process's working directory, which is usually
   but not always the executable's directory, or `dxmt.conf` there when `mullion.conf` cannot be opened
2. `MULLION_CONFIG`, which holds `option=value` pairs and replaces values from the file
3. Mullion's built-in settings for known applications, for the options neither of the first two set

A section of the file applies to the executable whose file name it names. The `DXMT_` names of variables still work,
but empty and unset values are not treated the same way everywhere, so an investigation of an override records both
names.

Changing an option is a comparison: it shows that the option changes the result, not that it fixes the defect. The
options a run used belong in its record, above all those that change generated shaders, such as
`d3d11.sampleNaNToZero`, `d3d11.defuseFma` and `mullion.shaderMetalVersion`. {logging: the effective value and
source of every option, written once at start-up, with malformed and unknown options reported}.

## 4. Use Wine and Metal diagnostics

Wine, Metal and Mullion each have their own diagnostics, set separately.

**Wine.** `WINEDEBUG` selects Wine's debug channels
([wine(1)](https://github.com/wine-mirror/wine/blob/master/tools/wine/wine.man.in)).
`WINEDEBUG=+timestamp,+pid,+seh,+loaddll` shows exceptions and the modules each process loaded, which is where a
failure to start begins. `+all` produces too much to read and changes timing. `WINEDEBUG` does not change Mullion's
own level, which `MULLION_LOG_LEVEL` sets.

**Metal API validation.** `MTL_DEBUG_LAYER=1`, set in the environment Wine starts with, enables Metal's API
validation, which by default logs a detected error and stops
([Apple](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-api-usage)). Validation is slow,
so performance comparisons run without it. Metal's shader validation
([Apple](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-shader-usage)) also exists, and
has not been verified with Mullion's shaders.

**Metal HUD.** `MTL_HUD_ENABLED=1` shows frame times and GPU time over the game, `MTL_HUD_LOG_ENABLED=1` writes them
to the console each second, and `MTL_HUD_LOG_SHADER_ENABLED=1` logs each shader compilation
([Apple](https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance)). These
logs show whether frame time spikes coincide with shader compilation.

Validation of the test suite is described in [TESTING.md 6.1](TESTING.md#6-running-tests-and-reading-results):
`tests/run.py --validate` runs the tests under Metal's API and shader validation, and what the validation reports is
in each test's log.

## 5. Capture a Metal frame

Mullion has a path for writing a GPU trace that Xcode opens
([`mullion_capture.cpp`](../src/mullion/mullion_capture.cpp)). It has not yet been verified as a procedure under Wine:

- `MTL_CAPTURE_ENABLED=1` allows capture, as Apple
  [describes](https://developer.apple.com/documentation/xcode/capturing-a-metal-workload-programmatically)
- `MULLION_CAPTURE_EXECUTABLE` names the executable to capture, without `.exe`
- `MULLION_CAPTURE_FRAME` names the frame to capture, or F10 captures the next one
- `MULLION_CAPTURE_TRIGGER` names a file (a Unix or a Windows path) whose appearance captures the next frames, from
  outside the game (`echo 4 > /tmp/mullion-capture`); it may hold the number of frames, and Mullion deletes it
- `MULLION_CAPTURE_FRAMES` is how many frames a capture holds, 1 by default (frame generation needs at least 2)
- with `MTL_CAPTURE_ENABLED=1` a queue encodes on one thread (`MULLION_D3D12_ENCODE_THREADS` overrides it), because a
  capture copies every buffer and heap the CPU writes for each command buffer that uses them. For the same reason the
  D3D12 arguments the GPU reads are Metal buffers of 16 MB, then 32 MB and so on as a program uses more, rather than
  one of 1 GB
- the trace is written as a `.gputrace` in `MULLION_CAPTURE_PATH`, else in `MULLION_LOG_PATH`, or in the working
  directory when both are unset (or `MULLION_LOG_PATH` is `none`)

Both the D3D11 and the D3D12 queues take part. Metal captures only command buffers created and committed while the
capture runs, and a message naming the trace file does not show that the capture succeeded: {logging: capture start
and stop reported with Metal's result and error, verified by a trace opened in Xcode with the intended commands and
resources}. Capturing a Wine process from Xcode itself has not been verified.

## 6. Inspect shader conversion and caches

Three different things can be examined: the Direct3D bytecode the game supplied, the Metal IR Mullion converted it
to, and the pipelines Metal compiled from that.

**Dumps.** `MULLION_SHADER_DUMP_PATH` names a directory where Direct3D 12 writes, on selected failure paths and for
pipelines named at `trace`, the containers of the shaders involved, named by hash with a `.dxbc` extension whether
they hold DXBC or DXIL ([`d3d12_pipeline_graphics.cpp`](../src/d3d12/d3d12_pipeline_graphics.cpp)). Direct3D 11
writes `shader_dump_<hash>.cso` into the working directory on selected failure paths, only in builds with the
`mullion_debug` option ([`d3d11_pipeline_cache.cpp`](../src/d3d11/d3d11_pipeline_cache.cpp)). Some refusals happen
before any dump, and a dump file is checked before it is used as evidence. {logging: one dump format for both APIs,
holding the bytecode, the stages, the root signature, the conversion options and the converted output}.

**The converter.** With the `build_airconv_for_windows` option, the build also makes
[`airconv`](../src/airconv/airconv_cli.cpp), a Windows program that converts a shader outside the game:
`airconv <shader> -S -o <output>` writes LLVM assembly, and `-A` a Metal library. Its conversion options are fixed,
while a game's come from the device and the configuration, so its output is not necessarily what the game ran.
Converted IR passes the LLVM verifier before optimization ([CODE.md 4.1](CODE.md#4-shaders)), except with `-O0`,
whose output is unverified and is checked with the verifier before anything else reads it.

**What the converter leaves out.** `MULLION_AIRCONV_SKIP` holds names, separated by commas, of paths the converter
leaves out of every shader, to find which one a shader goes wrong with or what one costs; each name is described where
it is read in the source. It is part of the shader cache's key. `custom-border` leaves out the path that samples a
border color Metal lacks, so such a sampler reads a border Metal has, and `anisotropic-gradients` lets an anisotropic
sampler with a bias read Metal's own level of detail, whose bias Metal drops once the footprint passes the sampler's
anisotropy. `MULLION_TRACE_SHADER=1` names the samplers that need either path.

**Caches.** Mullion keeps its converted shaders in a cache of its own
([`mullion_shader_cache.cpp`](../src/mullion/mullion_shader_cache.cpp)). `MULLION_SHADER_CACHE=0` turns it off, and
`MULLION_SHADER_CACHE_PATH` moves it to another absolute macOS directory. Metal keeps a separate cache of compiled
pipelines, which `MULLION_USE_DEFAULT_METAL_CACHE=1` leaves at Metal's default ([`dxgi.cpp`](../src/dxgi/dxgi.cpp)).
Turning Mullion's cache off therefore does not make every compilation cold. A comparison uses a separate cache
directory, so that shared caches are not deleted.

## 7. Read a hang or a slow frame

**A hang** is read from its thread stacks before its code. They are taken while the hang lasts, from both sides of
the bridge, with the loaded modules recorded so that the stacks can be matched to their binaries:

- Windows threads: `winedbg`, attached to the game's Windows process ID, then `info thread` to list the threads and
  `bt <thread-id>` for the stack of each
  ([winedbg(1)](https://github.com/wine-mirror/wine/blob/master/programs/winedbg/winedbg.man.in))
- native threads: the game's macOS process, found by its process ID, sampled with Activity Monitor's
  [Sample Process](https://support.apple.com/en-us/guide/activity-monitor/actmntr2225/mac), or `thread backtrace all`
  in [LLDB](https://lldb.llvm.org/use/map.html)

`tests/run.py` collects both for a test that outlives its time: the test's log then says where each thread of its
program was, and a sample of its processes' macOS stacks is written beside the log.

Mullion adds its own observations. At `trace`, the D3D12 queue logs each submission with its time and thread, and a
command buffer still awaited after ten seconds is reported with its Metal status
([`d3d12_command_queue.cpp`](../src/d3d12/d3d12_command_queue.cpp)). That report shows a long wait, not necessarily a
deadlock. `MULLION_D3D12_GPU_ERRORS=1` asks Metal for each encoder's execution status, for D3D11 as well despite its
name ([`winemetal_unix.c`](../src/winemetal/unix/winemetal_unix.c)), and `MULLION_TRACE_ENCODERS=1` logs each D3D11
render pass at `info` ([`mullion_diag.hpp`](../src/mullion/mullion_diag.hpp)). `MULLION_D3D12_ISOLATE=1`, with
`MULLION_LOG_LEVEL=trace`, waits after each pass, which can locate a failing pass but also changes scheduling, and
can hide the synchronization fault being sought.

**The flight recorder** is always on ([`winemetal_thunks.c`](../src/winemetal/winemetal_thunks.c)). Each thread keeps
its last records in memory: D3D12 submissions, command buffers with their Metal identities and completion status,
fence signals and waits, presents, D3D11 command buffers, the device's removal and exceptions. A record costs its
thread about 10 ns, and nothing is written while the game runs. A removed device, a Metal command buffer error, a D3D12
command buffer still awaited after ten seconds, a feature level `D3D12CreateDevice` refuses (the record says which
level the device reaches), or an exception that ends a process unless it is caught writes the records in time order to `~/Library/Logs/Mullion/<executable>.flight`, or to `MULLION_DIAG_PATH` when it is set, and
Wine's console output names the file. A process that ends after such a dump adds what was recorded since, and
`MULLION_FLIGHT=exit` writes one at the end of every process, for a failure Mullion does not see.

**A slow frame** is profiled with `MULLION_DIAG=profile` and `MULLION_DIAG_PATH`, a macOS directory, and read with
[`tools/mullion-diag`](../tools/mullion-diag): `mullion-diag report <directory>` lists frame times and where their
time goes, and `mullion-diag frames <directory>` summarizes the present intervals of each recording. Its own
description lists what the records do not cover. {logging: one offline report that gathers a run's logs, profiles,
stacks and shader dumps}.

## 8. Bisect a regression

A bisection needs a failure that repeats and a revision known to be good. The application, the workload, the
configuration and the diagnostics stay the same throughout, in a checkout and build used for nothing else.
[`git bisect`](https://git-scm.com/docs/git-bisect) does the search: a revision that does not build or cannot run the
reproduction is marked with `git bisect skip`, never as good, and `git bisect log` keeps the record, skips included.
Each revision is built with its own dependencies, the libraries it loaded are checked, and each runs with Mullion's
shader cache off or fresh, since the cache is versioned by the converter and not by the revision. A faster revision
counts as good only when it also renders correctly ([TESTING.md 10](TESTING.md#10-performance)).
{suite: running one fixed case against each revision}.

The first bad commit is where the observed behaviour changed. It is not necessarily the whole cause, and skipped
revisions next to it can leave several candidates.

## 9. Reduce the problem to a test

A reduction keeps what causes the failure and removes the rest: the API calls, descriptors, shader inputs, resource
relationships, queue order, reuse and sizes that tell failure from success. A rendering reduction reads the pixels
back, and a synchronization reduction keeps the dependency that fails, not a delay that happens to expose it.

`MULLION_D3D12_DUMP_FRAMES=<first>:<count>` writes the back buffers of a run's frames as raw `.frame` files under
`MULLION_LOG_PATH`, and [`tools/frames.py`](../tools/frames.py) finds regions that change for one frame and change
back in the next. It does not report corruption that persists, or changes that are small, so empty output does not
show that rendering was correct. Neither shows the commands that drew a frame.

The reduction becomes a test written as [TESTING.md 5](TESTING.md#5-writing-a-test) describes, seen failing before
the fix: it is registered in its directory's `meson.build`, and its control is a fault in `tests/breaks` that
`tests/faults.py` shows caught ([TESTING.md 7.1](TESTING.md#7-failing-controls-and-repeats)).
## 10. Behaviour switches in the environment

Nineteen names the code reads from the environment appeared in no document -- several explained only in a comment at
their own call site, which is no help to someone choosing what to set. Each row below was read at that call site, and
the file is named so the description can be checked rather than trusted. They are here because a switch nobody can
find is a switch nobody can use to answer a question, and because three of them -- `MULLION_D3D12_METAL4`,
`MULLION_ENABLE_NVEXT`, `MULLION_D3D12_PARALLEL_COMMANDS` -- are what the test suite itself sets, so a contributor
reading a failing test meets them there first.

**Taking a Metal frame capture.** All six are described in [section 5](#5-capture-a-metal-frame) as well; listed here
so they are findable from a name rather than only from a procedure.

| name | what it does | unset |
|---|---|---|
| `MTL_CAPTURE_ENABLED` | `1` is what Metal itself requires before any GPU capture is possible (`mullion_capture.cpp:31`) | no capture is attempted |
| `MULLION_CAPTURE_EXECUTABLE` | the process name a capture applies to: capture is off unless this matches the running executable's base name (`:32`) | off |
| `MULLION_CAPTURE_FRAME` | which frame to capture (`:61`) | none scheduled |
| `MULLION_CAPTURE_FRAMES` | how many frames a capture holds; a trigger file may carry the number instead (`:55`) | 1 |
| `MULLION_CAPTURE_PATH` | where the `.gputrace` goes; a trace is large, so failing this the log directory is used when `MULLION_LOG_PATH` names one, and never the executable's own directory (`:208`) | the log directory |
| `MULLION_CAPTURE_TRIGGER` | a file whose appearance starts the capture and which is then deleted, so a capture can begin from outside the game (`:65`) | no external trigger |

**Direct3D 12 command processing.**

| name | what it does | default |
|---|---|---|
| `MULLION_D3D12_METAL4` | `0` keeps Direct3D 12 on the Metal 3 bridge paths; anything else uses Metal 4 where macOS offers it. This is what a `-metal3` family in `tests/run.py` sets (`d3d12_device.hpp:871`) | Metal 4 where available |
| `MULLION_D3D12_PARALLEL_COMMANDS` | how many threads may encode one submission's independent pass runs at once; `1` encodes on the queue's own thread, which is what the tests use to make ordering observable (`d3d12_command_queue.cpp:377`) | sized by the machine |
| `MULLION_D3D12_THREADS_AT_ONCE` | the same limit given directly, for a test or to see what a part's size does | derived |
| `MULLION_D3D12_ENCODE_THREADS` | `1` encodes a frame on one thread (`d3d12_command_queue.cpp`, the comment above the two above) | parallel |
| `MULLION_D3D12_GPU_WAITS` | `1` lets a queue wait for another's completion event on the GPU. Off because that stalled ~4 s in one title and, past Metal's limit of five waits, silently dropped about a frame in fifty (`d3d12_command_queue.cpp`) | CPU waits |
| `MULLION_D3D12_METAL3_EVENT_WAIT` | on the Metal 3 path, wait by shared event rather than by command-buffer completion; a wrong prediction of which buffer a `Signal` follows costs time, not order (`d3d12_command_queue.cpp:107`) | on, Metal 3 only |
| `MULLION_D3D12_ALIASES_INSIDE` | `0` orders aliased resources with a barrier against all of the queue's preceding work instead of inside the next encoder (`d3d12_command_queue.cpp`, `MTL4CommandBuffer_orderAliases`) | inside |
| `MULLION_D3D12_HOIST_COPIES` | `0` stops BC4/BC5 SNORM companion decoding from being hoisted to the start of the render pass that reads it, after the last copy wrote (`d3d12_command_list.cpp:336`) | hoisted |
| `MULLION_D3D12_DEFER_PIPELINES` | `0` builds the Metal pipeline on the calling thread; `draw` waits until a draw needs it; any other value defers as broadly as the type allows (`d3d12_deferred_pipeline.hpp:64,71`) | deferred |
| `MULLION_D3D11_FAULT_DEVICE` | fails the nth map of a staging buffer, so a device can be lost at a chosen point in a frame (`1` is the first; a larger number leaves later maps whole) (`d3d11_device.cpp`) | nothing faulted |

**Rasterization and shader paths.**

| name | what it does | default |
|---|---|---|
| `MULLION_D3D12_PIXEL_CULL` | `all` culls in the pixel shader wherever that is possible, `0` never; otherwise only where the pixel shader cannot already be doing the culling -- not where it may discard, writes depth, or uses UAVs (`d3d12_pipeline_graphics.cpp`) | heuristic |
| `MULLION_D3D12_CULL_MESH_ONLY` | `0` keeps the object stage everywhere. Otherwise a triangle list whose vertex shader only passes cull distances through runs the vertex shader alone, which costs about what the triangles do (`d3d12_pipeline_graphics.cpp:1892`); strips, stream output, conservative rasterization, quadrilateral lines and geometry shaders keep the object stage regardless | mesh-only lists |
| `MULLION_D3D12_FAST_MESH` | `0` keeps the copy `DispatchMesh` made, for A/B runs against airconv's in-place payload build; the flag is part of the shader cache's key (`d3d12_pipeline_graphics.cpp`) | in place |
| `MULLION_D3D12_FAST_STREAM_OUTPUT` | `0` stops stream output running in the single pass that relies on each object threadgroup's primitives following from its place (`d3d12_pipeline_graphics.cpp`) | one pass |
| `MULLION_D3D12_SPECIALIZED_RAYS` | `0` makes pipelines call every shader through the function table, which is slower but traceable and lets the pipeline say what shader kinds it has (`d3d12_device.cpp`) | specialized |
| `MULLION_D3D12_FLEXIBLE` | `1` asks for Metal 4 pipeline specialization -- off by default because on macOS 27.3 a specialization leaves the fragment stage without a primitive id, and several tests fail through it (`d3d12_device.cpp`) | off |
| `MULLION_MAX_MESH_THREADGROUPS` | mesh threadgroups per draw, each holding 2 KB: sets the payload-and-geometry cap per draw; unset takes the family's published limit, 4 GB on Apple7 and Apple8, which 2^21 groups fill (`mullion_info.cpp:99`) | per family |
| `MULLION_ENABLE_NVEXT` | `1` turns the NGX bridge on, `0` off; with neither, it follows the configuration: a program whose section reports an NVIDIA vendor id (`dxgi.customVendorId = 10de`) gets it. A launcher that rewrites the environment leaves that per-program section alone (`dxgi.cpp:45`) | by configuration |

`MULLION_AIRCONV_SKIP` and `MULLION_AIRCONV_HOIST_LOADS` are converter switches, described in
[section 6](#6-inspect-shader-conversion-and-caches) with the shaders they affect; `DXMT_` spellings of some names are
read as fallbacks from the project's earlier name.
