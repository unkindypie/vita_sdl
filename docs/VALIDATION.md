# Validation — offline runtime 0.3

## Performance/art recovery — 2026-10-12

Plan: `GPU_REWRITE_PLAN.md`, recovery tasks 1–4. Technical summary and limitations:
`docs/PERFORMANCE_RECOVERY.md`. The target is now 60 FPS on real Vita; hardware
acceptance is still open. These are **emulator results**, not device predictions.

Normal-JS/audio, controlled input, no parity harness, surface-sync OFF:

| Run | Frames | Wall p50/p95 ms | JS p50/p95 ms | Wall elapsed |
| --- | ---: | ---: | ---: | ---: |
| Qt unpaced JS | 3600 | 5.27 / 7.93 | 0.011 / 0.023 | 20.5 s |
| Qt unpaced no-JS | 3600 | 5.14 / 7.75 | 0 / 0 | 19.9 s |
| SDL unpaced JS | 3600 | 5.17 / 7.80 | 0.011 / 0.022 | 20.0 s |
| Qt paced, host VSync ON | 600 | 16.66 / 16.71 | 0.018 / 0.045 | 10.1 s |
| Qt paced, host VSync OFF | 600 | 16.66 / 16.72 | 0.018 / 0.045 | 10.1 s |
| Qt ordinary real-time, idle | 1798 | 16.66 / 16.71 | 0.019 / 0.047 | 30 s |

Qt JS/no-JS use the same render revision. SDL and paced comparisons include
the later CDRAM texture/depth placement; do not attribute frontend differences
to JS or hardware throughput. Final stage-remainder telemetry was added after
these profiles. Routes move/rotate initially, then idle: not a worst-case combat
benchmark or long soak. No other active game/emulator workloads in the final
paired runs; earlier contended measurements are identified in the recovery doc.
The last row is a separate ordinary `--seconds, 30` launch, not smoke mode:
real simulation timestamps, no synthetic input, JS/audio enabled, zero script
errors/dropped audio, clean shutdown. It rendered 1798 frames in 30 seconds
(~59.9 FPS overall). Backend remainder was 2.81/5.00 ms p50/p95; present wait
median 10.98 ms. This idle check does not prove worst-case movement/combat.

Commands: `make vita3k-bench`; equivalent no-JS launch uses Qt with
`VITA3K_ARGS='--smoke, --frames, 3600, --unpaced, --no-js'` and the finite runner.
SDL uses the same frame/input/JS args. Paced runs omit `--unpaced` and explicitly
set `VITA3K_HOST_VSYNC=ON/OFF`. Raw logs:
`/tmp/asciicker-recovery-wrapped-qt-js.log`,
`/tmp/asciicker-recovery-wrapped-qt-nojs.log`,
`/tmp/asciicker-recovery-final-sdl-js.log`,
`/tmp/asciicker-recovery-final-qt-paced-on.log`,
`/tmp/asciicker-recovery-final-qt-paced-off.log`.
The ordinary-time run is `/tmp/asciicker-recovery-realtime-qt.log` (guest profile
summary is authoritative in the latest appended stage-log section).

Validation:

- `make test`: 18/18, including rejection of unlimited unpaced runs and the
  finite-runner stale-log/timeout/own-process cleanup tests.
- `make test-asan`: 18/18 (32.43 s); existing external SDL/EGL leak exception
  unchanged, all memory-access checks active. Log:
  `/tmp/asciicker-recovery-completed-asan.log`.
- `make gpu-test`: actual radeonsi GPU depth, rotated-instance lighting,
  material/mixed/mesh-half-block fixtures, composition and atlas updates passed.
  204,000 nonboundary display pixels checked per resolve fixture at this desktop
  window size. Log: `/tmp/asciicker-recovery-rotated-accelerated.log`.
- `make vita3k-gpu-test`: native rotated-normal lighting/depth/reuse/display
  checks at three target sizes; world/material/mixed/mesh resolve, composition,
  partial atlas updates passed. 458,752 pixels per native resolve fixture.
  Log: `/tmp/asciicker-recovery-rotated-native.log`.
- Linux host 600-frame gameplay parity launch/capture: PARITY_OK, SMOKE_OK and
  clean shutdown. Screenshot `build/linux/recovery-final.png` inspected.
- Final native 600-frame parity/reset run: PARITY_OK, GAMEPLAY_CHECKS_OK,
  SCREENSHOT_GPU_OK, SMOKE_OK and SHUTDOWN_OK. Native screenshot inspected.
  Expected injected-script errors: 8; this is not a benchmark.
  Log: `/tmp/asciicker-recovery-completed-native-parity.log`.
- VPK manifest verification: 257 required entries present. The compiler remains
  an external prerequisite. Generated logs/captures/VPK stay ignored.

Native reflection transfer is removed; sprite-underlay transfer is intentionally
retained after a GPU-blit replacement failed the rear-depth check. No failed
prototype/fallback code remains. The emulator launcher now checks the selected
profile compiler before launch; plain `make vita3k` remains interactive.
`game_ui_p50/p95` computes the per-frame remainder outside backend dispatch,
including JS/AI/physics/UI, before taking percentiles. No physical-clock changes,
quality reductions, disabled gameplay features or CPU scene fallback were used.
Physical 60 FPS, exact old-renderer visual parity, multi-sprite transparent
layering, sustained worst-case routes and the long Linux soak remain open.

## Hardware feedback repair — 2026-10-12

User reports 3–4 FPS and missing entities on physical Vita. This fails hardware
acceptance despite prior Linux/emulator success. Native terrain depth now uses
explicit force-store before the sprite scene force-loads it (VitaSDK GXM
depth surface contract). The geometry fixture checks both occluded and visible
dynamic geometry at three target sizes; native Vita3K passed all three.

Startup now has an asset-independent native LOADING/progress framebuffer before
SDL and shader compilation. It is released after first gameplay presentation;
startup errors also release display ownership safely. Linux remains unchanged.
Immutable native vertex/index buffers use CDRAM; offscreen entity images are
rejected before per-cell iteration. These optimizations are not a claim of
improved physical FPS. Bounded live profiles persist every 120 frames under
`ux0:data/asciicker/stages.log`, without requiring shutdown.

`make test`: 16/16 passed with depth regression/loader integration.
`make run ARGS='--smoke --frames 240 --screenshot build/linux/depth-fix.bmp'`:
SMOKE_OK, clean shutdown, no script errors; screenshot inspected, player/NPC
sprites visible. Host p50 1.53 ms is not hardware performance evidence.
`make vita`: VPK compiled/packaged. Physical loader appearance, missing-entity
repair and FPS remain unverified. Request the live profile log after hardware
play before selecting further GPU/simulation optimizations.

Final offscreen-culling build: `make test` 16/16 passed (15.09 s).
`make vita3k-gpu-test VITA3K_DIR=build/vita3k-shader-probe` passed native
depth/reuse/display/dynamic checks at three sizes, world fixture, both ASCII
material fixtures, composition and partial surface update checks. Generated
logs: `/tmp/asciicker-depth-native-fixture.log` and
`/tmp/asciicker-depth-perf-final-tests.log`.
`make vita3k-smoke VITA3K_DIR=build/vita3k-shader-probe`: 600 frames,
PARITY_OK, GAMEPLAY_CHECKS_OK, SCREENSHOT_GPU_OK, SMOKE_OK and SHUTDOWN_OK.
Live timing records were written during the run. The intrusive harness's
16.66 ms emulator median does not establish physical performance.

Recorded 2026-10-10. The latest request authorized all offline phases. Linux
and Vita3K substitute for device checks; real hardware acceptance remains open.

## Current commands and outcomes

### GPU rewrite Stage 1 / compiler prerequisite (2026-10-11)

The user reports **2–3 FPS on a physical Vita**. Device/clock settings and
hardware stage timings are not yet supplied. The active staged rewrite is in
`GPU_REWRITE_PLAN.md`; Stage 1 is a structural extraction, not a GPU scene or
an FPS fix. Stage 2 begins with an explicit native shader-compiler probe.

- `make test`: **11/11 pass**. Added independent engine contract validation,
  per-renderer scene/camera/picking dispatch, lifecycle/failure/isolation tests,
  and explicit Linux rejection of the Vita-only compiler probe. Existing
  gameplay, scripting, audio, packaging and glyph tests remain enabled.
- `make test-asan`: **11/11 pass**, no sanitizer finding.
- `make gpu-test`: accelerated OpenGL glyph readback still matches CPU output.
  This remains a glyph test, not new GPU scene validation.
- `make run ARGS='--smoke --frames 240 --screenshot build/linux/engine-final.bmp'`:
  movement/camera/grounded `SMOKE_OK`, 243 script calls, zero errors/audio drops,
  clean shutdown. The final framebuffer is byte-identical to the saved
  pre-refactor run; SHA-256
  `aba931de294994746e30a566feed342678179b59556e2e7fcd2dccfa53c97770`.
  Inspected the matching Linux capture. Timings were collected during parallel
  builds/tests and are not used to claim performance improvement.
- `make verify-vpk`: Vita cross-compilation/packaging succeeds; **257 required
  entries** verified, including the replacement indexed photo icon.
- `make vita3k-smoke VITA3K=/tmp/asciicker-vita3k/Vita3K VITA3K_DIR=build/vita3k-engine-stage1-final`:
  guest logs `PARITY_OK`, `GAMEPLAY_CHECKS_OK`, 600-frame `SMOKE_OK` and
  `SHUTDOWN_OK`. Eight script exceptions are intentional parity injections;
  zero dropped audio commands. Normal CPU play does not require the compiler.
- `make vita3k VITA3K=/tmp/asciicker-vita3k/Vita3K VITA3K_FRONTEND=sdl VITA3K_DIR=build/vita3k-shader-probe VITA3K_ARGS='--probe-gpu'`:
  expected nonzero exit and guest `GPU_SHADER_PROBE_ERROR Missing
  ur0:data/libshacccg.suprx` with installation guidance. This verifies the
  missing-prerequisite path only. Compilation success, GXM drawing and physical
  Vita performance are **not verified**; a legitimate user-supplied module is
  needed for native shader testing. No module was downloaded/extracted.

Fresh managed emulator setup currently fails HTTP 404 for the original pinned
GitHub asset. These runs used the existing cached executable with embedded build
ID `b75d5e62`, not an unchecked newer release. A fresh-install pin update remains
pending. Native shader diagnostics use only weak public VitaSDK SceShaccCg
stubs from the existing pinned image; no new compiler-wrapper code is vendored.

### Real-device installation icon fix (2026-10-11)

The previous packaged bubble icon was 128x128 RGBA, not indexed PNG. Following
the user's VitaShell `0x8010113D` report, replaced it with a square crop from
the user-supplied `packaging/IMG_4564.JPG`. The original external JPEG is left
untouched. Conversion strips icon metadata and produces a non-interlaced,
128x128, 8-bit indexed PNG; reproduction instructions are in
`packaging/README.md`. The source JPEG is not packaged.

`make verify-vpk` passes (257 required entries). Inspecting the VPK's actual
`sce_sys/icon0.png` confirms the required format and a SHA-256 identical to
`packaging/icon.png`. CMake now rejects an incompatible source icon during
configuration; host tests cover the valid icon and rejection of the source
JPEG. `make test`: **8/8 pass**. Real-device reinstallation is still needed to confirm the reported error
is resolved; emulator acceptance alone is not proof.

### Softer pastel presentation (2026-10-10)

- Added a shared constexpr display palette: slate instead of pure black,
  reduced saturation and gentler highlights, without changing scene lighting,
  glyphs, simulation or resolution. CPU final-texture scaling is linear and
  generated Vita3K configuration uses bilinear display filtering.
- `make test`: **6/6 pass**, including original palette preservation, graded
  endpoints/all entries, ordered colour ramps, padded/unaligned rasterization,
  all coverage values and software GPU/CPU parity.
- `make gpu-test`: accelerated OpenGL framebuffer readback matches the graded
  CPU reference exactly.
- `make run ARGS='--smoke --frames 240 --screenshot build/linux/pastel.bmp'`:
  `SMOKE_OK`, zero script errors/audio drops, clean shutdown. Inspected captured
  framebuffer: muted greens, lifted dark marks, readable glyphs and HUD.
  Median total 6.75 ms, scene 5.18 ms, glyph 0.93 ms; this single unmatched run
  is a smoke diagnostic, not evidence of a performance change.
- `make vita`: VPK builds successfully. Real Vita appearance remains unverified.
- `make vita3k VITA3K_DIR=build/vita3k-pastel-preview VITA3K_ARGS='--smoke, --frames, 240'`:
  generated configuration confirms `Bilinear`; guest logs `SMOKE_OK`, zero
  script errors/audio drops and `SHUTDOWN_OK`. Median total 27.46 ms, glyph
  1.70 ms (unmatched diagnostic run, not a controlled FPS comparison).

### GPU atlas experiment (2026-10-10)

Optional `--gpu-presenter` uses two real SDL/GXM geometry passes instead of CPU
pixel expansion/upload. CPU scene rasterization is unchanged. It is **not** an
all-GPU scene engine, and the CPU presenter remains the default because no
repeatable useful end-to-end gain has been established. Full-scene migration is
unfinished; see `GPU_RENDERING.md` for its concrete stages and dependencies.

- `make test`: **6/6** pass, including actual SDL software-geometry framebuffer
  readback vs CPU reference for all glyphs/colors, repeated frames, real atlas,
  full grid, invalid fonts and geometry limits.
- `make test-asan`: **6/6** pass, no sanitizer report.
- `make gpu-test`: actual host **OpenGL accelerated=1** renderer readback is
  byte-identical to the CPU reference; not just a geometry-construction test.
- `make run ARGS='--smoke --frames 600 --gpu-presenter --screenshot build/linux/gpu-reference.bmp'`:
  successful launch, movement/camera/grounded checks, script/audio and shutdown.
  GPU mode captures are explicitly labelled CPU-reference images, not GPU proof.
- Vita VPK builds. `make vita3k VITA3K_FRONTEND=sdl VITA3K_DIR=build/vita3k-gpu-final VITA3K_ARGS='--smoke, --frames, 600, --gpu-presenter, --verify-gameplay'`:
  real GXM path completes `PARITY_OK`, `GAMEPLAY_CHECKS_OK`, `SMOKE_OK` and
  `SHUTDOWN_OK`; 449 JS calls/eight injected exceptions, zero audio queue drops.

Initial matched 600-frame no-harness Vita3K runs: optimized CPU median **24.54 ms**,
GPU atlas **24.76 ms**. Final GPU repeat after removing unused CPU framebuffer
allocations: **25.01 ms** median / **30.73 ms** p95, scene 22.59 ms, vertex
preparation 0.61 ms, submission 1.59 ms, present 0.13 ms. Final CPU comparison
showed host variability (27.51 ms median / 50.02 ms p95 despite similar 22.86 ms
scene time), so that slower CPU run is not used to claim a GPU speedup.
An uncontended CPU repeat measured **24.70 ms** median / **30.41 ms** p95,
confirming that the final 25.01-ms GPU run did not establish an improvement.
Both finish all 600 frames with zero JS errors, zero audio queue drops and clean
shutdown. Individual stage medians do not add up to total median; CPU scheduling
and event work also affect total time. Benchmarks exclude gameplay parity and
GPU readback. Physical Vita performance remains unmeasured.
The Qt interactive `--gpu-presenter` launch was also visually inspected via an
actual emulator-window capture (`build/vita3k-gpu-preview/window.png`): real map,
characters, colored CP437 glyphs and footer are visible with nearest filtering.
This is a visual check, not an additional fixed-route performance comparison.

### CPU rendering / sharpness follow-up (2026-10-10)

Matched Vita3K **SDL** runs used the pinned emulator, fresh profile directories,
120x68 cells, JavaScript/audio enabled, seed 1, synthetic 16,667-us simulation
steps and 600 frames, **without** the intrusive gameplay harness:

```sh
make vita3k VITA3K_FRONTEND=sdl VITA3K_DIR=build/vita3k-perf-floor \
  VITA3K_ARGS='--smoke, --frames, 600, --screenshot, ux0:data/asciicker/perf.bmp'
```

| Wall-clock stage (ms) | Before | Final |
| --- | ---: | ---: |
| Total median | 28.09 | 24.54 |
| Total p95 | 34.77 | 30.21 |
| Scene median | 23.14 | 22.46 |
| Glyph conversion median | 4.50 | 1.66 |

This is about **13% lower median frame time / 14% higher median-equivalent
throughput**, not a guarantee of that gain in every scene or Qt's displayed FPS.
An intermediate run before the floor conversion measured 25.66 ms total median
and 23.57 ms scene median; compiler settings alone did not establish a scene
speedup. Final JavaScript median/p95: 0.020/0.046 ms, 603 calls, zero errors;
zero audio queue drops. Final run: `SMOKE_OK`, clean shutdown, 15.5 seconds.
Final/baseline Vita framebuffer SHA-256 are identical:
`64da4c90cb590e57f601c9b2141ff4971d75be8758d42f1f1c0315f91e92209e`.

Linux before/final fixed-route framebuffers are also byte-identical:
`15eedfe29544ad840c77a404f23041f599424eae8e1a330d70eb80125e9f9687`.
Linux total median was 6.10 ms before / 6.13 ms final: **no desktop speedup
claim**. The branchless converter trades a host glyph regression (0.55 to
0.93 ms) for a much larger ARM/JIT improvement. A contended 8.72-ms desktop
run during the sanitizer rebuild is excluded. Results are individual measured
runs, not a statistical hardware performance certification.

`make test` and `make test-asan` pass **5/5** after the final floor conversion,
including full palette/binary-frame/unaligned/padding comparisons, all alpha
values and float/double floor boundaries. Vita builds/packages successfully.
Final `make vita3k-smoke VITA3K_DIR=build/vita3k-perf-parity` completes actual
guest combat/inventory/menu/JS/audio checks, `SMOKE_OK` and `SHUTDOWN_OK`:
449 script calls, eight intentionally injected errors, zero audio drops. Its
29.67-ms median includes the harness/reload and is not a performance comparison.

Both SDL texture filtering and the emulator display filter now explicitly use
nearest-neighbor sampling; the native cell resolution/font/assets are unchanged.
The Qt frontend reports `Nearest`. Host-vsync changes were explored but not
retained: no isolated gameplay benefit was established. No new 60-FPS or real
Vita hardware claim is made; CPU scene rendering remains dominant.

Interactive frontend correction (2026-10-10): `make vita3k` now uses Qt
**without `--console`**, with explicit controller-emulation keyboard bindings.
Qt plus console initially crashed at `sceAudioOutOpenPort`; upstream console
startup skips SDL initialization, so that combination is not supported here.
The corrected launch reached audio/presentation and displayed the actual game.
Host keyboard W changed guest position from (0,15) to (59.54,86.66), with no
synthetic smoke controls enabled. The user subsequently confirmed manual input
was working. Mouse forwarding is provided by Qt's touch handlers; no separate
automated mouse assertion was completed in this pass. Host CTest now passes
**5/5**, including generated emulator config/key-binding regression coverage;
Vita VPK build passes. SDL smoke remains explicitly selected by `vita3k-smoke`;
the supplementary run this pass ended before guest `SMOKE_OK`, so its host
zero exit code is **not** recorded as a new complete smoke pass. Earlier complete
SDL smoke results below remain historical evidence. No uncontended Qt performance
baseline or real-device performance claim is made.

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
# Native shader compiler success-path check — 2026-10-11

Following explicit user authorization, downloaded the public
[AnimMouse/SceShaccCg module](https://github.com/AnimMouse/SceShaccCg)
at commit `f9511fc20ce905704f384b3a99a17c5e97f9a92c`. SHA-256 matched
`188adb8c34c3983d63244c32d981fd7b518a8dcfd200f5886233fface6b5fa40`.
Stored only under ignored `build/tools/shader-compiler/` and the local
`build/vita3k-shader-probe/storage/ur0/data/` profile; no firmware extraction,
repository asset, or VPK inclusion. Public availability/checksum verification
does not independently establish redistribution rights.

Command: `make vita3k VITA3K=/tmp/asciicker-vita3k/Vita3K VITA3K_FRONTEND=sdl VITA3K_DIR=build/vita3k-shader-probe VITA3K_ARGS='--probe-gpu'`.
Vita compilation/packaging and emulator execution completed successfully.
Latest guest stage log:
`GPU_SHADER_PROBE_OK vertex_bytes=340 fragment_bytes=212 GXM_program_check=pass`.
Host output: `/tmp/asciicker-shader-probe-installed.log`.
This validates native runtime compilation and program structure only, not GPU
drawing, visual parity, or physical Vita performance. No application source
changed for this check; prior host test results remain applicable.

## GPU graphics ownership and terrain extraction — 2026-10-11

Implemented independent native GXM/OpenGL geometry buffers, color/depth targets,
startup shader compilation, local-U16 batch packing, GPU-only display resolve
and explicit resource/queue ownership. The CPU renderer is not called by these
diagnostics. Removed the unused `asciicker_core` CMake compatibility alias;
factored the probe/compiler into one implementation, shared native GXP storage
across devices and updated the plan to remove legacy rasterization after the
playable replacement is validated (not maintain two shipping backends).

Commands/results:

- `make test`: **15/15 pass**, including software-Mesa actual GPU geometry,
  display-buffer pixels and full-map terrain rendering, plus existing gameplay,
  script/audio/asset checks. Log `/tmp/asciicker-gpu-final-tests.log`.
- `make test-asan`: **15/15 pass**. GPU fixtures retain ASan bounds/UAF checks,
  but disable leak detection only for those two EGL/llvmpipe tests: an initial
  unsuppressed run found 5,856 bytes retained by SDL3/offscreen driver/JIT
  allocations after teardown. Other tests retain leak detection. Do not claim
  leak-free native/GL ownership from that result. Log
  `/tmp/asciicker-gpu-final-asan.log`.
- `make gpu-test`: pass on AMD Radeon/radeonsi under Wayland; glyph reference,
  native-style geometry/depth/display fixture and real terrain. Inspect captures
  under `build/linux/gpu-fixture.bmp` and `gpu-terrain.bmp`. Log
  `/tmp/asciicker-gpu-final-host-gpu.log`.
- `make run ARGS='--smoke --frames 240 --screenshot build/linux/gpu-migration-gameplay.bmp'`:
  `SMOKE_OK`, scripts 243 / errors 0, audio dropped 0, clean shutdown. Inspected
  framebuffer: normal pastel ASCII map, HUD and characters still render. This
  is legacy playable-game regression evidence, **not** GPU game performance.
  Log `/tmp/asciicker-gpu-stage3-gameplay.log`.
- `make verify-vpk`: Vita compilation/packaging and **257 required entries**
  pass. Log `/tmp/asciicker-gpu-final-vpk.log`.
- `make vita3k-gpu-test VITA3K=/tmp/asciicker-vita3k/Vita3K VITA3K_DIR=build/vita3k-shader-probe`:
  pass with cached exact emulator build `b75d5e62`, OpenGL, surface sync enabled
  explicitly for readback, and the locally installed compiler. The default
  emulator asset still returns 404; no fresh-download success is claimed. Log
  `/tmp/asciicker-gpu-final-vita3k.log` and latest guest stage-log sections.

Geometry fixture checks near-first/far-last depth, rotation, separate local-U16
draw ranges, invalid-upload rejection, row padding/orientation, deterministic
target reuse, GPU display pixels and 127x93 -> 244x140 -> 127x93 transactional
recreation. Shared geometry instances are transformed in vertex shaders; checks
cover duplicate resource references, translated/offscreen draws and rejection
of nonexistent resources. Final native stages report `depth=pass orientation=pass
reuse=pass display=pass instances=pass` for all three targets and
`GPU_FIXTURE_OK frames=360`.

Two failures were diagnosed rather than bypassed: immediate address recycling
after freeing a smaller target produced black readback on this emulator; target
replacement now retains the old target until its replacement validates. GXM
display texture bounds now clamp/inset to valid texels, avoiding dark border
samples from padded storage. Wayland does not retain readable front-buffer
contents after swap, so the Linux diagnostic replays only the GPU resolve and
captures the actual window draw buffer before swapping it; no scene simulation
or CPU rasterization is replayed.

Real-map terrain results: all **4,876 patches**, **102 spatial chunks**,
**121,900 vertices**, **156,032 triangles** uploaded once; original height,
diagonal, visual and dark metadata verified against the loaded map. Both GPU
backends report **29,114 covered pixels**; final 244x140 terrain captures have
zero differing pixels. Rotated 244x140 triangle captures show small raster-edge
differences between GL and GXM; feature/landmark checks pass, but arbitrary GPU
pixel identity is not asserted. Earlier 127x93 triangle captures matched.

Matched 120-frame terrain diagnostic (not the playable game): Linux AMD
submit/wait p50 **0.41 ms**, p95 **3.96 ms**, collection **28.12 ms**, upload
**6.16 ms**; Vita3K native GXM p50 **16.64 ms**, p95 **16.67 ms**, collection
**136.23 ms**, upload **16.26 ms**. Native presentation waits for vblank; GL
submission is asynchronous and is not a GPU execution timer. Surface-sync
diagnostics and uncontrolled host contention also prevent using these figures
as shipping/hardware FPS comparisons. No CPU readback occurs in either normal
presentation path; fixtures perform explicit diagnostic captures outside the
measured frame loop.

Remaining: production camera/query and update/render decoupling, meshes,
material-driven ASCII/lighting, water/reflections, sprites/UI composition and
GPU-enabled gameplay parity. Normal game rendering is still CPU-based during
migration. Physical Vita shader boot, visual checks and >=30 FPS acceptance
remain **unverified**; no hardware-performance improvement is claimed.
# GPU ASCII/material stage — 2026-10-11

The world diagnostic now draws terrain and static meshes through GL/GXM and
resolves CP437 material glyphs, foreground/background and pastel colors on GPU.
It is not the playable GPU game: animated sprites, water/reflections, gameplay
proximity/UI integration, update separation and hardware profiling remain.
Normal gameplay still uses its temporary legacy renderer. No hardware speedup
or full feature parity is claimed by this entry.

- `make test`: 15/15 pass, including real-map surface-atlas gutters, exhaustive
  material/font extraction and independent orthographic/perspective camera
  projection/rays. Terrain picking works before any renderer exists.
- `make test-asan`: 15/15 pass after fixing upstream `World::Rebuild`'s scratch
  allocation leak when no instance uses the BSP tree. The documented external
  EGL/JIT leak-detection exception still applies to GPU tests only.
- `make gpu-test`: accelerated AMD/radeonsi world and ASCII pixel checks pass.
  Material-only resolve checked 866,140 nonboundary display pixels on this
  desktop's window size. The four-sample majority regression also runs in the
  headless world fixture. Pixel expectations come from source font/material
  texels, not a copied CPU scene renderer.
- `make run ARGS='--smoke --frames 240 --screenshot build/linux/ascii-migration-smoke.bmp'`:
  `SMOKE_OK`, movement/camera/grounded pass, 243 JS calls, zero script errors or
  dropped audio commands, clean shutdown. Framebuffer inspected: original
  playable ASCII scene/HUD still intact. This is the legacy gameplay check,
  **not a GPU gameplay benchmark** (total p50 6.70 ms, p95 8.36 ms on this host).
- `make verify-vpk`: VPK builds; 257 required entries verified.
- Native GXM world/ASCII checks pass in cached Vita3K via the documented
  `VITA3K`/`VITA3K_DIR` override. Material-only fixture checked 458,752 display
  pixels at 960x544; captures inspected. Native world has 117 resources,
  1,083 mesh instances, 49 visible draws, 22218 terrain metadata pixels and
  48 deferred mesh lines. Recorded submit+wait p50 16.64 ms / p95 16.68 ms
  includes emulator display synchronization; it is not real-Vita GPU time.
- Initial native ASCII validation failed with white terrain. Inspection of the
  pinned emulator's `bind_texture_without_cache` showed that every sampler
  shared `cache.textures[0]`. Ordinary texture caching now stays enabled,
  hashless caching stays disabled, and the config test guards both settings.
  The identical native shader then passed the source-texel pixel checks.
- Two subsequent emulator launches crashed before the plain geometry fixture
  completed (`Unhandled SIGSEGV`, trap). The surface shader declared a sampler
  even when the fixture selected only direct colors. Both backends now bind a
  valid neutral fallback texture: shader compilation may hoist sampling outside
  the selection branch. With this fix, the entire native target passes again:
  all three target sizes plus world and both uniform/mixed-material ASCII
  checks. Each ASCII check validates 458,752 pixels at 960x544. Latest successful
  log: `/tmp/asciicker-ascii-bound-vita3k.log`. No retry loop or suppressed error
  was added to the launcher.

Logs/captures remain ignored under `/tmp/asciicker-ascii-*`, `build/linux/`
and `build/vita3k-shader-probe/storage/ux0/data/asciicker/`. Latest appended
guest stage sections, not old successes in the same log, are authoritative.
## Playable GPU integration — 2026-10-11

- `make test`: 16/16 pass after adding `gpu_playable_parity`, which runs the real
  GPU scene on offscreen Mesa with gameplay/JS/audio checks and New Game/reset.
- `make gpu-test`: actual AMD/radeonsi material checks and new front/rear sprite
  depth, transparency and overlay pixel checks pass. Shared shader extraction
  builds through CMake on Linux and Vita.
- `make run ARGS='--gpu-scene --smoke --frames 600 --verify-gameplay --screenshot build/linux/cell-pass.bmp'`:
  gameplay parity, movement/camera/grounding and clean shutdown pass. Captures
  inspected for CP437 terrain/meshes, player/NPCs, HUD and pastel water.
  This diagnostic intentionally injects script failures and resets gameplay;
  its maximum frame time and error count are not normal-play benchmarks.
- Vita3K synchronized 240-frame GPU run completes with `SMOKE_OK` and clean
  shutdown. The two-pass ASCII version measured total median 60.22 ms / p95
  78.10 ms, including emulator synchronization; this is not real-Vita speed.
  Full native parity/reload, latest feature validation and unsynchronized matched
  performance checks are still pending. Do not infer success from process exit.
- Shader/source logs and framebuffer captures are ignored under `build/` and
  `/tmp`. No physical-device performance or suspend/resume checks were performed.

### Follow-up: shader files, native reload and profiling

- `make test`: 16/16 pass with dirty-atlas bounds, row stride and neighbouring
  texel checks in the real GPU fixture (`/tmp/asciicker-surface-region-tests.log`).
- Native GXM 600-frame `--gpu-scene --smoke --verify-gameplay` now passes
  `PARITY_OK`, `GAMEPLAY_CHECKS_OK`, `SMOKE_OK` and `SHUTDOWN_OK`, including New
  Game/reload. GPU BMP inspected: ASCII terrain, mesh silhouettes, player/NPCs,
  water/reflections, HUD. Log: `/tmp/asciicker-native-vsync-off.log`.
- The prior reload stall was examined with a read-only debugger attach. The
  emulator render thread waited in Wayland swap/frame callbacks while guest
  `sceGxmFinish` waited for completion. Finite SDL smoke now disables **host**
  VSync; interactive Qt retains it. Native Vita display sync is unchanged.
- Matched Linux 600-frame routes with/without normal JS (no parity harness):
  total p50 2.93/2.92 ms, p95 4.31/4.26 ms; JS p50 0.007/0 ms,
  p95 0.020/0 ms. Zero script errors/dropped audio commands. AMD/radeonsi host,
  native builds were concurrently active: these are development measurements.
  Logs: `/tmp/asciicker-host-js-profile.log`, `/tmp/asciicker-host-nojs-profile.log`.
- Cached Vita3K 4154, host VSync OFF, surface sync OFF, normal JS: total p50
  16.66 ms / p95 16.83 ms; JS p50 0.040 / p95 0.072 ms. GPU CPU-wall p50
  collection 0.96, reflection 2.30, primary 1.58, UI 0.18, present wait 5.56 ms.
  These stages overlap scene timing, include synchronization, and are **not**
  physical Vita timings. Log: `/tmp/asciicker-native-js-profile.log`.
- Official Vita3K 4168-46602447 candidate archive was authenticated against the
  GitHub release asset SHA-256; its no-JS 600-frame run passes. A matched JS,
  fixture and parity run must pass before replacing the setup pin. One candidate
  fixture launch failed visibly with a bounded QuickJS `InternalError:
  interrupted` during concurrent builds; it is not recorded as a successful run.

### GPU-only cleanup and corrected native fixtures

- The repeated cold fixture bootstrap interruption was reproduced without
  concurrent builds. Trusted bundled API initialization now shares one total
  1-second deadline; loaded scripts/console remain 50 ms and callbacks 5 ms.
  The full native world fixture then reaches rendering successfully. This is
  a startup-budget fix, not a retry loop or relaxed interactive script limit.
- The new 4x4 dirty-atlas fixture exposed native GXM's eight-pixel linear row
  alignment. Full and partial uploads now copy source rows into padded GPU rows.
  Native checks pass: real world, both 458,752-pixel uniform/mixed ASCII checks,
  sprite/UI composition and atlas bounds/stride/neighbour retention.
  Log: `/tmp/asciicker-native-stride-fixture.log`.
- Official 4168-46602447 passes 600-frame parity/reset and normal JS routes.
  `make vita3k-setup` successfully downloads/authenticates the replacement pin
  into the project-managed `build/tools/vita3k/`; validation no longer requires
  the old `/tmp` emulator checkout. Log: `/tmp/asciicker-refreshed-emulator-setup.log`.
- Removed the upstream CPU scene rasterizer and SDL glyph-only presenter/test;
  normal launch is now GPU-only. A small non-rasterizing camera/collision test
  double preserves headless gameplay/ownership coverage. Missing backends fail
  explicitly; no software fallback was retained. Historical CPU validation
  sections above describe superseded builds, not current launch options.
- `make test`: 16/16 pass after GPU-only cleanup and desktop letterboxing
  (`/tmp/asciicker-letterbox-tests.log`). `make test-asan`: 16/16 pass on the
  final formatted/letterboxed implementation (`/tmp/asciicker-gpu-letterbox-asan.log`).
  Existing external EGL/JIT leak exceptions apply only to GL tests.
- Default `make run ARGS='--smoke --frames 600 --verify-gameplay --screenshot build/linux/gpu-letterbox.bmp'`:
  parity/JS/audio/reset, movement/camera/grounding, GPU capture and shutdown pass.
  GPU framebuffer inspected at 1265x716 with intact Vita aspect and CP437 art.
  Desktop input uses the same letterbox transform; bar presses are ignored.
- Default managed-emulator `make vita3k-smoke VITA3K_DIR=build/vita3k-shader-probe`:
  native GPU parity/reset, capture, `SMOKE_OK` and clean shutdown pass. This uses
  the existing profile's separately installed compiler. Log:
  `/tmp/asciicker-gpu-letterbox-native.log`. Its diagnostic timing is not a normal
  gameplay benchmark and the HUD uses synthetic simulation time.
- `make verify-vpk`: 257 required entries verified after the GPU-only change
  (`/tmp/asciicker-gpu-only-vpk.log`). No physical Vita measurements or hardware
  touch/suspend checks were performed. Transparent overlapping sprite layering
  and comprehensive water/shoreline visual comparison remain open, as does the
  legacy game's update/UI orchestration split. These are not marked complete.
- Final `make gpu-test` passes on AMD/radeonsi after letterboxing: geometry,
  world, 819,764 nonboundary pixels per uniform/mixed material check, sprite/UI
  composition and atlas-region updates (`/tmp/asciicker-gpu-only-accelerated.log`).
  Final VPK verification again reports all 257 required entries present
  (`/tmp/asciicker-gpu-only-final-vpk.log`).

## Icon and Linux/Vita visual comparison — 2026-10-12

- Added `packaging/IMG_6612.PNG` and converted it to the required 128x128,
  indexed, non-interlaced `icon.png`. Transparency is flattened onto black.
  Previous generated icon preserved as `icon-previous.png`; original JPEG and
  SVG retained. Only the active icon enters the VPK.
- Matched normal-JS/audio smoke captures at frames 240 and 600, Linux offscreen
  OpenGL and native GXM/Vita3K, both 960x544. ImageMagick normalized RGB RMSE:
  0.00115825 and 0.00115589. Inspected captures and highlighted difference
  images; no broad lighting/gamma discrepancy reproduced in these scenes.
- Vita3K final display defaults to Nearest, matching Linux. Bilinear remains
  explicit opt-in. Config tests cover default, override and invalid filters.
- `make test`: 18/18 passed. `make verify-vpk`: 257 required entries present.
  Host launch and native 240/600-frame runs reached `SMOKE_OK` and
  `SHUTDOWN_OK`; normal JS reported zero errors. Logs: `/tmp/visual-tests.log`,
  `/tmp/visual-vpk-final.log`, `/tmp/visual-tests-delivery2.log`, `/tmp/visual-linux-native.log`,
  `/tmp/visual-linux-600.log`, `/tmp/visual-vita-600.log`.
- Reusable `make visual-captures` documents the native-resolution comparison;
  its finite runner checks freshly appended completion markers even if SDL
  exits between polls (regression-tested alongside stale-marker rejection).
  End-to-end command passed: `/tmp/visual-captures-final.log`.
  see [visual parity](VISUAL_PARITY.md). Guest captures exclude emulator-window
  filtering. No claim of physical Vita visual/performance validation or full
  transparency/reflection parity follows from these two route endpoints.
