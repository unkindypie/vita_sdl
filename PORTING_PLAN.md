# Asciicker Linux / PS Vita Porting Plan

Status: offline runtime phases 0–4 implemented, 2026-10-10. Linux and actual
ARM/Vita3K gameplay parity checks pass. Real-device/public-release acceptance
remains explicitly unverified; this is version 0.3, not hardware certification.

## Current execution scope

The latest user request supersedes the Phase 2 stopping point: finish the
offline runtime, including JavaScript, audio, combat, inventory and menus;
continue Linux/emulator validation and compare performance after JavaScript.
Use official QuickJS 2026-06-04 on both targets. Hardware-only checks are still
deferred under the requested Linux/emulator substitution, not implicitly passed.
GPU work remains a measured decision gate, not a mandatory renderer rewrite.

Progress:

- Phase 0: completed for substitute scope. Pinned sources, both builds, probe,
  manifest and Docker tests pass. QuickJS now executes actual game callbacks
  and source scripts in Linux and the ARM Vita binary under Vita3K.
- Phase 1: completed. Real upstream scene, CPU ASCII presenter, shared SDL2
  launcher, input, and bounded profiling run on Linux. Simulation and CPU scene
  rendering are measured together because they share upstream `Game::Render`.
- Phase 2: implemented. VPK cross-compilation and packaging pass; Linux host
  smoke checks exercise movement, camera, grounded physics, and clean shutdown.
  Soak and supplementary emulator findings are recorded in `docs/VALIDATION.md`.
  The earlier no-JS 900-second soak completed (53,928 frames; then 3/3 tests,
  251 package entries). Current JS/audio validation supersedes those counts:
  53,986 frames over 900 seconds; normal/ASan 4/4; 257 package entries.
  Follow-up: a 600-frame Vita3K smoke passes with movement/camera/grounded
  checks, framebuffer capture, and clean shutdown using OpenGL with hashless
  caching off. Vulkan's texture-protection fault is avoided, not fixed upstream.
  This is not a declaration of real-device acceptance or 30 FPS on Vita.
- Phase 3: implemented and explicitly exercised on Linux/Vita3K. Full `ak`
  bridge, default per-frame script, console/controller keyboard, execution/
  stack/heap limits, safe New Game reset, SDL Ogg/mixer/playback, real combat,
  inventory ownership, menus and validated settings. See `docs/PARITY.md`,
  `docs/SCRIPTING.md`, `docs/AUDIO.md` and current `docs/VALIDATION.md`.
- Performance gate: retained the optimized CPU presenter. Scene work dominates;
  separate `js_p50/js_p95` diagnostics and matched JS-on/off runs monitor script
  cost. No evidence currently justifies replacing the whole scene renderer.
- Phase 4: offline candidate packaging/documentation complete: ASCIIV001,
  “Asciicker Vita” 00.30, original icon, manifest and notices, clean builds and
  emulator install. Hardware performance/suspend/physical audio and independent
  asset clearance remain external release gates, listed below and in parity docs.

Latest performance gate: JS-on Linux total median 6.16–6.30 ms, callback median
0.007–0.010 ms. Vita3K JS-on total median/p95 27.97/34.03 ms versus JS-off
28.17/34.66 ms; JS callback median/p95 0.020/0.046 ms. This is below observed
host/emulator variance, not a real-device performance claim. The JS/audio soak
reports zero script errors/audio drops and 52 KiB RSS drift after warm-up.

Follow-up CPU optimization: compile the ASCII presenter with `-O2` in Debug
as well as Release, and fast-path zero/full glyph coverage while preserving
antialiased blending. A Linux smoke measured glyph median 0.53 ms (previously
8.93 ms), total median 6.28 ms / p95 8.35 ms. This is not GPU rendering work.
See `docs/VITA3K.md` for reproducible emulator commands and limitations.

## Goal and scope

Port Asciicker's existing game runtime to Linux and PS Vita using this
repository's Docker, Make, CMake, and VPK workflow. The first major deliverable
is an interactive ASCII scene using the real engine, maps, terrain, sprites,
character movement, camera, and collision, in the style of the upstream demos.

`REQUIREMENTS.md` now describes this port, replacing the voice-training direction.
`AGENTS.md` follows the engine, native GXM SDL build, Linux tests, diagnostics,
bounded synchronous scripting and allocation-free audio callback practices.

Initial scope is the offline game runtime. The desktop editor, multiplayer,
new game content, and a replacement GPU scene renderer are later work.
Combat, inventory, menus, audio, and scripting parity are verified explicitly;
their presence in upstream does not make them automatically supported.

## Technical decisions

| Area | Baseline decision | Revisit when |
| --- | --- | --- |
| Scene rendering | Preserve upstream CPU renderer and ASCII-cell output | Vita profiling identifies an unacceptable scene-rendering bottleneck |
| Presentation | Shared SDL2 presenter; CPU glyph rasterization into a streaming texture first | Measured glyph conversion/upload cost justifies GPU glyph presentation |
| Scripting | QuickJS on both Linux and Vita, preserving the JavaScript `ak` API | Compatibility spike fails or essential scripts require unsupported behavior |
| Platform integration | Small adapters for paths, timing, lifecycle, input, and diagnostics | An upstream interface already provides the necessary abstraction |
| Tests | Headless CMake/CTest suite through Docker and `make test` | Hardware-specific behavior requires a separate manual check |
| Dependencies | Pin upstream engine and runtime revisions and toolchain image digests | A deliberate dependency update passes regression checks |

QuickJS's actual game bridge is now validated; it is not a general drop-in V8
host API replacement. Do not attempt to bring V8 to Vita for this runtime.
Lua/Python would require translating JavaScript and bindings, so they are not
the baseline. A scripting-disabled build is allowed for early bring-up only;
it must identify unavailable functionality and must not masquerade as parity.

## Architecture

```text
SDL input -> input adapter -> existing game / physics / world
                                      |
                              CPU scene renderer
                                      |
                               ASCII-cell buffer
                                      |
                           SDL2 presenter -> display

game callbacks <-> scripting adapter <-> QuickJS
game assets   <-> asset/path adapter <-> packaged files
game audio    <-> selected SDL2 playback backend
```

Keep upstream engine code recognizable and changes reviewable. Prefer a pinned
vendored source snapshot with origin/revision/license metadata and a documented
local patch list. Add the portable launcher separately rather than extending
the existing desktop launcher with many Vita conditionals.

Proposed boundaries, created only as needed:

- `third_party/asciicker/`: pinned engine sources and required upstream assets.
- `third_party/quickjs/`: pinned runtime sources and necessary Vita patches.
- `src/app/`: lifecycle and engine integration.
- `src/ui/`: ASCII presenter and reference cell-to-pixel conversion.
- `src/platform/`: platform paths, input, timing, and diagnostics.
- `src/scripting/`: runtime ownership, bindings, callback and exception handling.
- `tests/`: portable unit, integration, and script compatibility tests.
- `docs/`: rendering, scripting, controls, packaging, and hardware test notes.

CMake should define engine, presenter, scripting, launcher, and test targets
with explicit dependencies. Enable C as well as C++ for upstream `.c` sources.
Exclude the desktop editor, X11/terminal frontend, desktop GL loaders, V8,
PulseAudio-specific backend, and server implementation from the port target.
Provide explicit offline implementations for required networking symbols.

## Phase 0 — Reproducible foundation and dependency spikes

Tasks:

- [x] Pin Asciicker revision and inventory the game runtime's sources,
  dependencies, assets, global platform hooks, and license notices.
- [x] Replace historical voice requirements with port requirements and align
  `AGENTS.md` examples without discarding unrelated user edits.
- [x] Add headless CTest tests and Docker-backed `make test`; preserve
  `make linux`, `make run`, and `make vita` and host-owned build outputs.
- [x] Pin build images and document dependency acquisition. Normal compilation
  must not fetch source implicitly; dependency setup may require network access.
- [x] Build the minimal engine source set on Linux and cross-compile it for Vita.
  Track unresolved platform hooks rather than importing desktop dependencies.
- [x] Build a pinned QuickJS candidate on both platforms. Start with the existing
  QuickJS Vita port's patches; assess their applicability and maintenance needs.
- [x] Prove on Linux: JavaScript evaluation, native function calls, typed arrays, shared
  `ArrayBuffer` writes visible to C++, exception reporting, and teardown.
- [x] Define bundled asset roots and writable configuration roots. Audit
  relative file writes such as screenshots and debug exports.

Exit criteria:

- A clean checkout has documented dependency setup and reproducible build/test
  commands; headless tests pass in the Linux container.
- A minimal VPK launches and exits on real Vita hardware.
- The QuickJS feature probe passes on Linux and Vita, or a written blocker
  identifies the missing capability and next bounded experiment.

Do not claim the port is proven from linking alone. No full scripting adapter
or graphics optimization is required to exit this phase.

## Phase 1 — Reference ASCII presentation and Linux scene

Tasks:

- [x] Implement the palette and glyph conversion using upstream fonts and cell
  semantics, including background colors, glyph orientation, and blank cells.
- [x] Present a bounded pixel buffer through an SDL2 streaming texture; handle
  texture pitch, dimensions, errors, cleanup, and renderer capabilities.
- [x] Select an initial glyph/grid size experimentally. Start near 120x68 cells
  if compatible with the chosen font; use integer scaling or letterboxing to
  preserve glyph proportions on the 960x544 Vita screen.
- [x] Integrate upstream map loading, engine initialization, timing, scene
  updates, and CPU rendering into the new launcher.
- [x] Add keyboard and desktop controller movement/camera controls.
- [x] Use explicit scripting-disabled and offline adapters during bring-up if
  needed; document every behavior they omit.
- [x] Add timing for combined simulation/scene rendering, glyph conversion,
  texture update, and presentation; account for vsync waits separately.

Exit criteria:

- Linux displays an actual upstream map with terrain and character sprites.
- Movement, camera, and collision work; no canned screenshot or substituted
  renderer is used.
- Presenter unit tests and map-load integration tests pass headlessly.
- Missing assets produce a useful error instead of a crash.

## Phase 2 — First playable Vita milestone

Tasks:

- [x] Package the necessary map, meshes, sprites, font, palette, and notices in
  the VPK; verify every manifest entry. Emulator asset loading is supplementary.
- [x] Map sticks, D-pad, face buttons, shoulders, and start/select to a documented
  control scheme. Avoid depending on unavailable L2/R2/stick-click buttons.
- [x] Keep a usable exit path; test focus/input reset and resume rendering on Linux.
  Actual Vita suspend/resume is deferred by the user's runtime-test substitution.
- [ ] Measure frame times and memory on real hardware at the selected grid size
  and record device model, firmware, clock settings, build, and scene.
- [x] Inspect the Linux scene framebuffer, including terrain, objects, water,
  sprites, and movement. Real Vita visual comparison remains deferred.
- [x] Fix platform differences and bounds/ownership failures before adding
  optional functionality.

Exit criteria — first playable demo:

- An installable VPK shows the same kind of interactive scene as upstream demos.
- The player can move and turn the camera; terrain collision works.
- No missing glyphs, systematic palette errors, inverted output, or absent core
  scene assets are observed.
- A 15-minute exploration session completes without a crash, input lockup, or
  continuing memory growth.
- Aim for a stable 30 FPS baseline; report actual median, p95, and worst frame
  times. If this is missed, record a performance blocker and proceed to the
  measured optimization gate below before declaring the milestone accepted.

This milestone proves the engine and scene can run interactively on Vita.
It does not promise complete combat/inventory/script/audio/menu parity or the
desktop frame rate shown in reference screenshots.

## Phase 3 — JavaScript compatibility and gameplay parity

Tasks:

- [x] Replace V8-specific context creation, native registrations, evaluation,
  callbacks, and error handling with a QuickJS adapter.
- [x] Preserve the `ak` API and existing JavaScript bootstrap where compatible.
  Verify shared-buffer ownership, lifetime, bounds, typed-array layout, and
  UTF-8 string conversion explicitly.
- [x] Test position/direction/camera/movement setters and getters; water/light;
  mount/action; `say`, `jump`, `isGrounded`; and `onSay`, `onItem`, `onFrame`.
- [x] Check callback registration/removal, state reset on a new game, exceptions,
  repeated initialization, and shutdown. Add script execution/memory limits
  and verify an interrupted script leaves the application usable.
- [x] Support existing script-entry behavior; define a usable Vita text-entry
  route if the in-game scripting console is included in the release.
- [x] Select the SDL2 audio playback backend and validate mixing, sample loading,
  format, pause/resume, synchronization, and failure behavior on Linux/Vita3K.
  Physical speaker/suspend checks remain hardware-only.
- [x] Validate combat, inventory, interactions, menu navigation, configuration,
  and save/continue behavior where implemented upstream. Maintain a parity
  matrix with supported, deferred, and broken states backed by tests.

Evidence: shared `src/app/self_test.h`, CTest launch/scene/audio-fallback tests,
`make vita3k-smoke` with `GAMEPLAY_CHECKS_OK`, and `docs/PARITY.md`. Upstream
Continue means current-world resume, not campaign persistence; Profile and
fixed-grid zoom placeholders are hidden, not falsely implemented.

Exit criteria:

- Both platforms use the same pinned QuickJS revision and JavaScript API.
- Script compatibility tests pass; a runtime version of “set position, register
  callback, trigger action, observe result” works on Vita.
- Agreed offline gameplay features and audio work with controller input.
- Deferred features are clearly documented; no script-dependent feature is
  silently accepted with a no-op binding.

## Performance decision gate

Run after Phase 2 and repeat after gameplay integration if measurements change.

1. Establish reproducible scenes, grid sizes, hardware settings, and per-stage
   timings. Separate CPU work from GPU/vsync waiting.
2. If scene rendering dominates, inspect hot paths, allocation churn, culling,
   grid resolution, reflection cost, and double-precision use. Change precision
   only with numerical and visual regression coverage.
3. If glyph conversion or transfer dominates, prototype GPU presentation using
   batched font-atlas glyphs/backgrounds or a cell-buffer shader. Compare CPU
   cost, draw-call cost, upload cost, visual output, and dependencies with the
   reference streaming-texture path on Vita.
4. Adopt a GPU presenter only when end-to-end measurements demonstrate a useful
   improvement. Keep the reference converter available for tests.
5. Consider a GPU scene renderer only if measured CPU optimizations cannot meet
   the playable budget. Treat it as a separately planned renderer project with
   terrain, depth, sprites, palette/glyph selection, and reflection parity tests.

Do not assume that “GPU” guarantees better performance. Preserve SDL2 for
window/input/audio services if a later presenter needs a dedicated graphics API;
check backend interoperability rather than mixing graphics contexts blindly.

## Test and validation strategy

| Layer | Required checks | Execution |
| --- | --- | --- |
| Cell conversion | Known glyphs/colors, blanks, orientation, dimensions, invalid input, row pitch | Headless Linux unit tests |
| Engine integration | Packaged map/asset loading, controlled time/seed, valid cell output, repeated teardown | Headless Linux integration tests |
| Scripting | API results, callbacks, shared-buffer mutations, Unicode, exceptions, limits, reset | Linux tests plus Vita smoke harness |
| Input/path adapters | Axis deadzones, press/release, asset/config separation, missing files | Headless tests and manual hardware checks |
| Packaging/build | Linux compilation, Vita cross-compilation, required VPK asset manifest | Docker build checks |
| Runtime | Visual comparison, controls, audio, lifecycle, frame times, memory stability | Manual Linux and real Vita |

Use deterministic synthetic cells and small fixtures. Control randomness and
time for scene tests; allow justified numeric/visual tolerances across CPU
architectures instead of requiring universal exact scene hashes. Test externally
observable results, not copies of implementation logic. Keep benchmarks separate
from pass/fail unit tests and do not require display/audio hardware for CTest.

## Phase 4 — Release preparation

- [x] Finalize app name, project-specific title ID, version, icon, and provenance/
  license notices. ASCIIV001 is project metadata, not a globally registered ID.
- [x] Verify clean builds and isolated Vita3K VPK installation with documented
  commands; normal hardware installation remains unverified.
- [x] Complete the offline feature parity matrix and known-issues list.
- [x] Document controls, configuration location, dependency setup, and any
  additional on-device runtime requirements introduced by chosen libraries.
- [x] Repeat Linux tests and representative emulator sessions with the release
  Vita build under the user's device-test substitution.
- [ ] Actual hardware: representative sessions, speakers/headphones, lifecycle,
  memory/frame budget and stable 30 FPS; independent asset-rights clearance.

Release acceptance requires both platform builds, passing host tests, agreed
offline gameplay functionality, a usable controller UI, and recorded Vita
performance. A successful VPK build alone is insufficient.

## Risks and decision records

- **CPU budget:** software rendering may require a smaller ASCII grid or targeted
  optimizations; hardware measurements decide.
- **Script compatibility:** QuickJS support for JavaScript does not establish
  compatibility with V8-specific embedding or shared-memory behavior.
- **Upstream coupling:** launcher globals and platform hooks may require more
  extraction than expected; track patches and avoid broad rewrites.
- **Asset/path assumptions:** debug writes and relative paths can fail in an
  installed VPK; audit before hardware acceptance.
- **Dependencies/licenses:** review engine, runtime, fonts, sounds, and individual
  bundled assets before distributing them; retain required notices.
- **Hardware access:** real Vita validation is mandatory. Until it is available,
  record affected milestones as unverified rather than inferring success.

Record choices and benchmark results in feature documents under `docs/` as
implementation proceeds. Each completed task should link to its implementation,
test, or hardware evidence. Do not mark a phase complete while its exit criteria
remain unverified.

## Research baseline

Source inspection used Asciicker commit
`8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635`. No engine compilation or Vita benchmark
had been performed when this plan was written.

- [Asciicker engine](https://github.com/msokalski/asciicker)
- [CPU renderer](https://github.com/msokalski/asciicker/blob/8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635/render.cpp)
- [JavaScript API](https://github.com/msokalski/asciicker/blob/8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635/game_api.cpp)
- [Native launcher and V8 bindings](https://github.com/msokalski/asciicker/blob/8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635/game_app.cpp)
- [QuickJS Vita candidate](https://github.com/MrProcastinator/quickjs-vita)
- [SDL2 Vita documentation](https://wiki.libsdl.org/SDL2/README-vita)
- [VitaGL](https://github.com/Rinnegatamante/vitaGL)
