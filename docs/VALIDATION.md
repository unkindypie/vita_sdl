# Phase 0–2 validation

Recorded 2026-10-10. Linux runtime checks replace Vita runtime acceptance for
this implementation pass, as requested. No Phase 3 work was started.

## Commands and results

| Check | Result |
| --- | --- |
| `make test` | 3/3 CTest tests pass: pure presenter/input/QuickJS plus repeated real-scene load/render/free; SDL launch with synthetic input; missing-assets failure |
| `make test-asan` | Same 3/3 pass with AddressSanitizer, including teardown and focus reset/resume |
| `make run ARGS='--probe-js'` | `QUICKJS_PROBE_OK`: evaluation, native function, typed array/shared buffer, expected exception, teardown |
| `make run ARGS='--smoke --frames 240 --screenshot build/linux/smoke.bmp'` | Desktop launch, `SMOKE_OK movement=1 camera=1 grounded=1`, `SHUTDOWN_OK` |
| `make verify-vpk` | Vita cross-compilation succeeds; all 251 required package entries present |

Desktop smoke used Wayland/SDL's OpenGL presentation backend. Inspection of the
960x544 framebuffer showed the actual upstream scene: terrain, water, trees,
characters, health bars, and readable preview/controls footer. The engine still
renders on the CPU; SDL's desktop GL backend does not introduce a GL dependency
into the shared engine.

Final desktop smoke timings (240 frames, Linux Debug, optimized upstream core):
total median 15.73 ms, p95 17.52 ms, maximum 22.54 ms; median combined
simulation/scene 5.51 ms, glyph conversion 9.04 ms, upload 0.78 ms, present
0.24 ms. These measurements are affected by host load and backend scheduling;
they are not Vita estimates. The displayed 60 FPS uses synthetic smoke time.

## Long Linux run

Command: `SDL_VIDEODRIVER=dummy build/linux/vita_sdl2 --smoke --seconds 900`.
This is an automated movement/camera burst followed by an idle-scene soak,
not 15 minutes of manual exploration or comprehensive collision coverage.
The run began before the final texture lock/row-copy change; final presenter
and focus changes are covered by the subsequent normal/ASan and desktop smoke
runs above.

Outcome: `SMOKE_OK frames=53928 movement=1 camera=1 grounded=1 elapsed_s=900.0`
followed by `SHUTDOWN_OK`. No crash or hang occurred. RSS was 33,924 KiB at
startup, 34,244 KiB at 60 seconds, and 34,296 KiB at the last minute sample
(840 seconds). The 52 KiB upward drift after warm-up is small, but this is not
proof of zero growth over unlimited sessions. AddressSanitizer reports no
failure in the separate regression tests. A longer allocation-profiled run
would be needed to classify that drift conclusively.

The last 3,600-frame window reported total median 16.66 ms, p95 16.67 ms,
maximum 24.92 ms; scene median 4.50 ms, glyph 8.96 ms, upload 0.21 ms,
present 2.89 ms. Dummy/software presentation and frame pacing affect these
figures; they are not a Vita benchmark.

## Supplementary Vita3K attempt

Used official Linux Vita3K 0.2.1, build `4154-b75d5e62`, downloaded into `/tmp`;
its emulator/storage files are not repository dependencies. Installed the VPK
with the SDL frontend and an isolated preference directory. Both Vulkan and
OpenGL attempts reached map/sprite/mesh loading and engine initialization.
Final stage logs reached `render.ready` and `present.ready` before a later
host SIGSEGV in `sceClibMemcpy` / libc `memmove`. Disabling texture caching or
changing mapping settings did not establish a successful sustained run.

Initial result: **emulator runtime blocker** (resolved by the configuration
workaround below). At that point there was
not enough evidence to attribute it solely to Vita3K or to prove the application
safe on hardware. Vita3K's memcpy HLE directly copies translated pointers;
the observed fault is at that boundary ([upstream implementation](https://github.com/Vita3K/Vita3K/blob/master/vita3k/modules/SceLibKernel/SceLibKernel.cpp)).
The follow-up remained limited to emulator configuration and CPU presenter
optimization, without scripting or GPU renderer integration.

Vita stage diagnostics append to `ux0:data/asciicker/stages.log`. The intended
stdio files (`port.log` / `error.log`) remained empty during these emulator
attempts, so use the raw stage file and emulator log for startup diagnosis.
Logs in `/tmp/asciicker-vita3k-*.log` and the emulator's normal cache log contain
the local investigation; these generated artifacts are not committed.

## Follow-up: performance and successful Vita3K run

The user reported Linux medians of scene 5.99 ms and glyph 8.93 ms, with total
15.40 ms / p95 16.71 ms. The glyph loop was still compiled without optimization
in Debug. Added `-O2` for the presenter (retaining symbols) and direct color
selection for binary coverage, preserving antialiased blending. Added exhaustive
coverage-value tests with unaligned output. Normal and ASan suites each pass
3/3 after these changes, and `make verify-vpk` still verifies 251 entries.

Final Linux host smoke: 240 frames, movement/camera/grounded checks all pass,
framebuffer inspected, clean shutdown. Total median 6.28 ms, p95 8.35 ms,
maximum 24.08 ms; scene 5.33 ms, glyph 0.53 ms, upload 0.25 ms, present 0.18 ms.
The runs are not perfectly controlled benchmarks, but show the previous
presenter overhead is no longer dominant. RSS around 158 MiB includes desktop
graphics-driver resources and is not comparable to the dummy-driver soak.
Focus-loss time is intentionally excluded from rendered-frame profiling; wall
time divided by frames across a paused session is not its rendering FPS.

Vita3K succeeds with **OpenGL, hashless texture cache off, texture cache off,
memory mapping disabled**. Earlier CLI backend overrides did not establish that
the intended backend was actually active; the passing run explicitly selects
OpenGL in the loaded YAML. GDB showed the Vulkan failure was `SEGV_ACCERR`
writing a read-only texture-memory page at the memcpy HLE boundary. No engine
memory-copy workaround or emulator binary patch was necessary.

Guest run: `--smoke, --frames, 600, --screenshot, ux0:data/asciicker/vita3k.bmp`.
The Vita ARM binary logged:

```text
FRAME_PROGRESS frame=600 pos=-43.46,35.81,12.85 yaw=153.63
PROFILE frames=600 total_p50=28.29 total_p95=36.08 total_max=99.88 scene_p50=23.31 glyph_p50=4.34 upload_p50=0.38 present_p50=0.07 ms
SMOKE_OK frames=600 movement=1 camera=1 grounded=1 elapsed_s=18.1
SHUTDOWN_OK scene/resources released
```

The guest framebuffer was inspected and contains the real terrain, water,
trees, characters, HUD, and footer with expected glyph orientation/colors.
Emulator timings reflect ARM emulation and host GPU scheduling, not Vita FPS.
The displayed 60 FPS remains synthetic smoke time. See `docs/VITA3K.md` for
the repeatable Make targets. Raw progress/profile/smoke/shutdown records were
added because guest stdout/stderr files were not useful in this setup.

The new `make vita3k-smoke VITA3K=/path/to/Vita3K` target was also run using
fresh repository-local emulator storage and its generated minimal configuration.
It independently passed: 600 frames, all three checks 1, clean shutdown,
18.4 seconds elapsed (median 28.64 ms / p95 36.75 ms). A subsequent 30-second
guest smoke was used to inspect and capture the actual emulator window, not
just the CPU framebuffer; it visibly presents the expected animated scene
with aspect-preserving letterboxing. Generated capture: `build/vita3k/window.png`.

## Remaining limits

- No real Vita runtime, performance, controller, or suspend/resume validation.
- QuickJS compiles for Vita; its feature probe has only been executed on Linux.
- Controller mappings are unit-tested and synthetic axis input is exercised;
  physical controller ergonomics have not been checked.
- Full scripting, audio, gameplay/menu/save parity, networking, GPU optimization,
  and public-release asset attribution remain deferred.

## Regressions fixed during bring-up

- Repeated scene loads retained stale sprite pointers in the upstream static
  item prototype initializer. Refreshing that table on each load fixes the
  failure; repeated load/render/free tests cover it.
- Portable menu exit now requests normal launcher shutdown rather than calling
  `exit` before resource cleanup.
- D-pad release reaches keyboard-equivalent movement even when an inventory /
  menu transition occurs after its press, preventing a held movement key.
- Texture upload locks the SDL streaming texture and honors its actual pitch;
  this avoids assuming contiguous backend rows but did not solve Vita3K's fault.
