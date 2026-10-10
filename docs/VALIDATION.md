# Validation — offline runtime 0.3

Recorded 2026-10-10. The latest request authorized all offline phases. Linux
and Vita3K substitute for device checks; real hardware acceptance remains open.

## Current commands and outcomes

Project-managed emulator setup (2026-10-10): `make vita3k-setup` fetched
official asset 625596737 into `build/tools/vita3k/` and passed SHA-256 validation.
A subsequent setup reused the installed emulator without downloading. A missing
explicit binary override failed with an actionable message; shell syntax check
passed. Plain `make vita3k-smoke` (no `VITA3K` override or `/tmp` dependency)
built the Vita VPK and completed 600 guest frames with `GAMEPLAY_CHECKS_OK`,
`SMOKE_OK`, and `SHUTDOWN_OK`. Generated tool files are ignored by Git.

| Check | Recorded result |
| --- | --- |
| `make test` | 4/4 pass: presenter/input/probe and repeated real-scene/gameplay checks; SDL parity launch; missing assets; unavailable-audio fallback |
| `make test-asan` | Same 4/4 pass, no sanitizer failure, including repeated JS/world/audio ownership teardown |
| `make test LINUX_BUILD_DIR=build/linux-fresh CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release` | Fresh-directory native Release build/tests pass |
| `make verify-vpk VITA_BUILD_DIR=build/vita-fresh` | Fresh-directory Vita Release build and packaging pass |
| `make verify-vpk` | 257 required entries: 249 assets plus metadata/icon/manifest/notices; app “Asciicker Vita”, ASCIIV001, 00.30, approximately 5.1 MiB |
| `make run ARGS='--smoke --frames 600 --verify-gameplay --screenshot build/linux/parity-final.bmp'` | `NEW_GAME_OK`, `PARITY_OK`, `GAMEPLAY_CHECKS_OK`, all movement/camera/grounded checks, `SHUTDOWN_OK` |
| `make vita3k-smoke VITA3K=/path/to/Vita3K VITA3K_DIR=build/vita3k-final` | Fresh isolated install, then same full guest parity and clean shutdown; not merely host process success |

Current guest parity run: 600 frames, all movement/camera/grounded checks true;
449 script calls, 8 **intentionally injected** exceptions, 134,150 bytes of
runtime-accounted JS memory, zero dropped audio commands and 673 nonzero PCM
callbacks. Controller tests cover inventory equip/unequip, attacks/crossbow,
console submission, Continue and perspective/blood/mute menu actions. Real
melee damage, item ownership/script veto/mutation, New Game JS-state reset,
bounds/Unicode/stack/OOM/time-limit recovery and config replacement/rejection
are assertions, not inferred from a scene launch. See `docs/PARITY.md`.

The intrusive parity run's 1,347.98 ms worst frame includes full world/runtime/
sample reload. Deliberate failing scripts run after the profiled render loop.
Do not use these correctness runs as ordinary-play performance benchmarks.

## JavaScript performance comparison

Default startup installs a real lightweight per-frame callback. Matched smoke
input, 120x68 grid, optimized core/presenter, SDL audio on; no parity harness.
Smoke initialization now uses a fixed timestamp/seed for repeatable NPC layout.
Displayed “60 FPS” is synthetic simulation time, not measured throughput.

| Target/run | Total median / p95 ms | JS median / p95 ms |
| --- | --- | --- |
| Linux Wayland/OpenGL, JS on, 600 frames (two later runs) | 6.16–6.30 / 7.83–8.37 | 0.007–0.010 / 0.019–0.020 |
| Same Linux JS off | 6.17–6.25 / 8.10 | 0 / 0 |
| Vita3K OpenGL, JS on, 1,200 frames | 27.97 / 34.03 | 0.020 / 0.046 |
| Same Vita3K JS off | 28.17 / 34.66 | 0 / 0 |

Linux timings are host-load-sensitive; an earlier pair overlapping compilation
was slower and is excluded from the representative ranges. Small total-time
differences do not imply JS makes rendering faster. Normal JS runs report
**zero script errors**; Vita3K JS-on reports 1,203 calls, 131,390 bytes of
runtime-accounted memory and zero audio drops. Default callback cost is below
observed frame-to-frame variation, not a guarantee for arbitrary future scripts.

Vita3K JS-on median scene work is 23.13 ms, glyph conversion 4.41 ms, upload
0.26 ms, present 0.06 ms. Scene timing includes JS. Startup maximum 102.98 ms
is recorded, not hidden. Scene work still dominates; preserve the optimized
CPU reference presenter rather than starting an unproven GPU renderer rewrite.
Neither emulator p95 meets a strict 33.33 ms frame budget; these are **not**
real Vita FPS/clock/memory measurements or stable-30-FPS acceptance.

## JS/audio Linux soak

`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/linux/vita_sdl2 --smoke --seconds 900`
completed **53,986 frames**, `SMOKE_OK` and clean shutdown. It uses a synthetic
movement/camera burst then an idle scene, not 15 minutes of manual exploration.
Zero script errors, zero audio queue drops, 38,997 nonzero PCM callbacks, final
JS memory accounting 154,562 bytes. RSS: 51,068 KiB startup, 51,272 at 60 s,
51,324 at 840 s: **52 KiB drift after the first minute**, not proof of no leak.

The latest 3,600-frame window measured total median 16.66 / p95 16.67 / max
17.53 ms; scene 4.70, glyph 0.56, upload 0.20, present 11.13 ms; JS median
0.008 / p95 0.018 ms. Dummy presentation pacing dominates total time. This run
preceded final input/menu/config hardening and fixed smoke seeding; those are
covered by later host/ASan and guest parity tests. Default JS/audio hot paths
are unchanged. This is stability evidence, not exhaustive gameplay coverage.

## Visual checks and emulator findings

Inspected the Linux framebuffer and the actual cropped emulator window:
terrain, water, trees/rocks, characters, health bars, ASCII palette and footer
are present. Generated evidence remains ignored under `build/linux/` and
`build/vita3k-final/window-final.png`; it is not committed source content.

Two integration issues were caught and corrected by emulator testing:

- Vita SDL's `SDL_StartTextInput()` opened native IME over normal gameplay.
  Vita now explicitly stops native text input and uses the existing controller/
  front-touch keyboard. A subsequent emulator window capture has no overlay.
- The tested Vita3K file-mode translator ignores `O_TRUNC`. Settings use a
  freshly created staging file followed by replacement; regression fixtures
  likewise create a real one-byte file. The guest explicitly verifies existing
  settings replacement and corrupt-file rejection without partial apply.

Front touch IDs are 1 (front) / 2 (rear) in SDL Vita; engine contact zero is
reserved for mouse. The adapter now maps three front fingers to contacts 1–3.
The guest smoke injects the first SDL finger and asserts it reaches contact 1.
OpenGL/no-hashless-cache remains the working emulator graphics baseline; see
`docs/VITA3K.md`. Physical speakers, controls, suspend/resume, memory/frame
budgets and independent asset-rights clearance remain external release gates.

## Historical Phase 0–2 evidence (superseded scope)

The records below describe the earlier scripting/audio-disabled preview only,
before the user authorized Phase 3–4. They do not describe current feature scope.

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
