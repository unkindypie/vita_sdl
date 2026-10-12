# GPU engine rewrite

Authorized 2026-10-11. This plan supersedes the old GPU-glyph experiment as the
active rendering roadmap. User-reported hardware baseline: **2–3 FPS** on a
physical Vita; workload/device/clock details and stage timings are not yet
recorded. A successful emulator run is not a hardware performance result.

## Outcome and gates

Make the existing offline game playable with GPU scene rendering, preserving
its ASCII identity and pastel presentation. Target sustained **60 FPS** at
960x544 output (user refinement 2026-10-12). Use the 120x68 cell
grid initially. Lower internal sampling, dynamic quality, caching and batching
are allowed only as explicit measured options, not hidden feature removal.

Keep CPU simulation, collision, AI, scripting, audio, inventory and combat.
Move geometry transformation/rasterization, depth, lighting/material lookup,
water/reflections, sprite occlusion and ASCII resolution to GPU passes.
The user refined scope to prefer a single clean GPU renderer (2026-10-11).
Legacy CPU rasterization and obsolete presenter experiments/switches are now
removed following host/native gameplay and material/ASCII checks. Remaining
visual audits and hardware performance are still explicit gates, not implied by
default selection. Normal GPU play must not run a CPU scene pass or read
back the scene/depth framebuffer. CPU simulation and UI generation remain.

Each stage must build Linux and Vita, pass relevant host tests, update its
status/evidence and leave the previous working backend usable. Visual parity
means explicit feature comparison; exact pixel identity is only required for
unchanged reference paths. Record intentional differences, never hide missing
geometry behind broad image-error tolerances.

## Performance and art recovery pass — 2026-10-12

Additional visual-parity follow-up: [matched native-resolution captures](docs/VISUAL_PARITY.md).
Two route endpoints have normalized RGB RMSE ~0.00116 between Linux and GXM;
no broad lighting mismatch reproduced. Emulator presentation now defaults to
Nearest, with Bilinear opt-in. Physical display and other scenes remain open.

Physical baseline remains 3–4 FPS; interactive Qt emulator ~20 FPS. Do not
extrapolate emulator/physical speed ratios. The hardware target is a 16.67 ms
frame budget; emulator benchmarks must distinguish guest display pacing,
frontend VSync and unpaced execution. No test-harness script failures in benchmarks.

1. [x] Record matched Qt normal-JS/audio runs with host VSync on/off, same
   scripted route and profile, plus SDL comparison. Preserve raw stage logs.
   Identify waits versus collection/JS; fix frontend pacing only if measured.
2. [x] Remove native reflection framebuffer transfer using owned target
   rotation, then reduce redundant synchronization without unsafe buffer reuse.
   Run native/GL depth, target-recreation, composition and gameplay checks.
3. [x] Restore authored mesh lighting (previously unlit RGB with a generic
   half-block conversion), improve ASCII surface detail and retain pastel
   contrast without flattening geometry. Add lighting/normal fixtures and
   inspect matched land/water/entity captures. Do not drop ASCII/reflections.
4. [x] Re-profile matched routes after changes; Linux tests, ASan, accelerated
   GPU fixtures, native emulator fixtures and gameplay parity. Document exact
   medians/p95, visible differences and remaining bottlenecks.
5. [ ] Final physical-Vita acceptance: visible player/entities, preserved art,
   60 FPS sustained movement/combat/water with JS/audio. This requires device
   evidence and cannot be checked off by emulator success.

Task order is profiling, native dataflow, art restoration, regression/finite routes,
then hardware acceptance. Keep this checklist current; no completion claims
while the physical 60 FPS gate remains unverified.

Evidence/details: [performance and art recovery](docs/PERFORMANCE_RECOVERY.md).
The sprite-underlay blit experiment failed native rear-depth checks and was
removed; its synchronized transfer remains. Reflection target rotation passed.
Initial final-code Qt headroom: 3600 frames, JS/audio enabled, median 5.27 ms,
p95 7.93 ms, wall elapsed 20.5 s (~176 frames/s overall). Matching no-JS median
5.14 ms/p95 7.75 ms; final SDL JS median 5.17 ms/p95 7.80 ms. These are emulator
results, not predictions for the physical device.
Final paced Qt comparisons with host VSync ON/OFF both reached median 16.66 ms
(p95 16.71/16.72 ms) after native resource changes. Earlier slow-path results
cannot be attributed solely to the host toggle. Mesh lighting/detail fixtures,
native captures and host/native gameplay checks passed; exact old-renderer visual
parity, sustained combat/worst-case routes and the long soak remain separate open
items. Marking this recovery pass does not close those older acceptance gates.

## Architecture

```
games/asciicker: world/assets, rules, combat, items, menus, script bindings
       | game adapter: render resources + immutable per-frame view
engine: rendering contracts, geometry/material resources, camera/picking,
        ASCII composition, reusable input/audio/script services over time
       | render backend
Linux OpenGL / Vita native GXM: buffers, targets, shaders, depth, synchronization
```

New reusable engine headers must not include Asciicker game types, SDL or Vita
headers. Adapter code may depend on both sides. Do not rename vendored files
into an engine and claim the gameplay coupling disappeared. The initial game
remains in `third_party/asciicker` with small documented integration patches.
Future platformer/roguelike games provide their own world and rules; reuse does
not require subclassing Asciicker's `Game`, item or physics classes.

### Stage 1 — Rendering seam and regression baseline

- [x] Introduce a standalone C++17 render-contract target, shared cell/view types
  and a per-renderer backend boundary, with no global backend singleton.
- [x] Route scene render and camera projection/picking through that boundary.
  Keep the null/default backend byte-identical to the existing CPU path.
- [x] Specify borrowed backend lifetime, copied frame metadata, exactly-once
  scene dispatch, and overlay/interaction ownership.
- [x] Test dispatch, camera delegation, invalid bounds, detach/reload and CPU
  fixed-route framebuffer identity. Run existing gameplay/JS/audio tests.
- [x] Build the Vita VPK. This stage is **not a GPU renderer or FPS improvement**.

### Stage 2 — Shader/toolchain and graphics ownership

- [x] Pin and document native GXM + shader compilation dependencies.
  User authorizes the separately installed `libshacccg.suprx` prerequisite.
  Do not redistribute Sony's module or extract firmware without instructions.
  Probe availability early and show an actionable error, not a black screen.
- [x] Add explicit `--probe-gpu` startup diagnostic with native module loading,
  vertex/fragment compilation and GXM program validation. Linux rejects this
  Vita-only probe explicitly. Both missing-module and compile-success behavior
  are tested in Vita3K with the separately installed module.
- [x] Own a dedicated GXM context/display lifecycle; never commandeer SDL's
  private renderer state. SDL remains responsible for input/audio/window events.
  Linux uses a matching dedicated GL renderer, with software Mesa for headless
  shader checks. Keep native APIs behind adapters.
- [x] Implement vertex/index buffers, depth, offscreen targets, shader error
  diagnostics and explicit pass state. Native shader compilation is once per
  graphics runtime; immutable GXP storage outlives all registered patchers.
  No disk cache until measured need; any future cache must key source/compiler.
- [x] Validate a depth-tested rotating geometry fixture, transactional target
  recreation, repeated teardown, texture orientation and actual GPU readback in
  tests only. Include independent local-U16 batches and display-buffer pixels.
- [x] Run geometry/display and real-terrain fixtures on Vita3K.
- [ ] Obtain an actual-Vita shader/boot check; emulator evidence is not hardware.

### Stage 3 — Real world geometry and independent gameplay queries

- [x] Extract complete terrain height/diagonal/visual/dark snapshots and create
  bounded spatial GPU batches. Test all 4,876 real-map patches; upload/draw once
  per fixture load without CPU scene rasterization or per-frame uploads.
- [x] Add backend-independent shared-geometry instance records and GPU model/
  view transforms, with resource/matrix bounds checks and native/GL fixtures.
- [x] Extract/cache real terrain heights, diagonal topology, visual/dark maps,
  mesh assets/instances and bounds through the Asciicker adapter. Upload static
  data once; dirty/version changes invalidate only affected resources.
- [x] Render visible terrain and meshes with depth and stable camera matrices,
  orthographic first, then architectural perspective. Keep reflected geometry
  separate from primary geometry. Cull/batch by chunk/material; use 16-bit
  index batches and bounded transient rings on Vita.
- [x] Remove gameplay's dependency on CPU sample-buffer depth. Pick with camera
  rays + terrain/world collision; proximity/target selection is game-owned and
  must work without a render or GPU readback. Preserve shooting and talk anchors.
- [ ] Extract simulation/update from `Game::Render` without changing timestamps,
  event order, script count or action transitions. One simulation per tick;
  renderer replay must never advance the game.
- [ ] Test real-map coverage, seams, negative coordinates, culling, mesh
  transforms, depth intersections, perspective and picking. No CPU rasterizer
  execution in the selected GPU scene path.

### Stage 4 — GPU material and ASCII shading

- [x] Render low-resolution material/normal/elevation information, starting at
  the existing 244x140 sample footprint (including its border).
- [x] Resolve the four samples per cell using material shade tables, direct
  mesh colours, edge/glyph selection and palette quantization in shaders/LUTs.
  This is material-driven ASCII, not an arbitrary grayscale ASCII filter.
- [x] Resolve CP437 coverage and the shared pastel palette into the output
  target. Preserve deterministic glyph sampling; final display softness is
  separate from glyph-atlas sampling.
- [x] Introduce the shared material/font texture contract and GPU ASCII resolve
  shader, real material-table/pastel/font extraction, and exhaustive host-side
  addressing/orientation/transactionality checks. The diagnostic checks actual
  resolved display pixels; this does not complete gameplay integration.
- [ ] Fixtures cover materials/lighting ramps, depth discontinuities, silhouettes,
  sample borders, glyph UVs and seams. Compare representative real-map captures.

### Stage 5 — Complete playable game integration

- [ ] GPU water, shoreline blending, ripples and reflected scene/sprites.
- [ ] Cached animated sprite resources; per-frame transforms, depth/alpha,
  player/NPC/item occlusion, interaction highlights, attacks and projectiles.
- [x] Composite independent CPU-generated menu/HUD/inventory/dialogue/console
  overlay cells without reading back GPU scene cells. Existing UI helpers can
  no longer assume they can sample/modify the scene under them; audit each use.
- [x] Verify combat, item pickup/equip/drop, inventory, menus, New Game/reset,
  JS callbacks/console and audio explicitly with GPU scene enabled.
- [ ] Linux and Vita3K captures at land, water, steep cliffs, tree/mesh overlap,
  combat and inventory. Obtain corresponding real-device visual checks.

### Stage 6 — Hardware performance and default selection

- [ ] Record Vita model, firmware, CPU/GPU clocks and settings. Measure matched
  routes with normal JS/audio, no parity injection; idle, movement, combat,
  water and menu transitions, warm-up and sustained play.
- [ ] Separate simulation/AI/JS, scene collection, GPU submission, overlay and
  wait timings. Report median/p95 and peak memory. Avoid double-counted timers.
- [ ] Optimize the actual limiting stage: static caches, visibility, draw-call
  merging, shader LUTs, target formats, overdraw, bounded uploads. CPU simulation
  may still need work; a GPU rewrite alone does not guarantee 60 FPS.
- [ ] Require sustained 60 FPS target route with no missing gameplay/features
  and acceptable frame pacing before claiming the hardware performance goal.
  GPU is the sole default under the user's no-legacy-renderer refinement.
- [x] Remove the superseded CPU rasterizer and presenter experiments once the
  GPU path passes its feature gates; keep a small GPU fixture for debugging.
  Add a second minimal game module only after the engine boundary is stable;
  a complete platformer/roguelike is not part of this renderer rewrite.

## Dependency decision

The supported direction is native GXM with runtime compilation through the
public SceShaccCg API and separately installed Sony compiler, not an undocumented
change of SDL2's backend. The initial probe uses that API directly (no vitaGL,
vitaShaRK or compiler-extension library needed). These weak stubs are supplied
by our existing digest-pinned VitaSDK image; there is no new downloaded build
dependency. The newer open-shacccg project is a research option, not the shipping
baseline: its own README still lists real-Vita validation and compatibility
gaps. Evaluate any switch separately with pinned sources and on-device tests.

References inspected 2026-10-11:

- [vitaShaRK](https://github.com/Rinnegatamante/vitaShaRK)
- [vitaGL compiler prerequisite](https://github.com/Rinnegatamante/vitaGL)
- [open-shacccg status](https://github.com/frangarcj/open-shacccg)

## Progress

- Stage 1 implemented: independent `engine_render` target, engine cell/view
  contracts, per-renderer dispatch/camera/picking/error boundary, Asciicker loader
  under `src/games/asciicker/`, game/input separated from presenter linkage.
- Stage 2 started: native `--probe-gpu` compiles into the VPK. Both the
  missing-compiler diagnostic and successful vertex/fragment compilation plus
  GXM program validation are verified in Vita3K. Following explicit user
  authorization, a checksum-verified community-distributed compiler was fetched
  into ignored local build/profile storage only; it is not bundled in the VPK.
  Native geometry/depth, GPU-only display and readback checks now pass on both
  platforms, including headless Mesa. Shader storage is shared across target
  recreation; no duplicate compiler implementation or target-name aliases.
- Stage 3: full terrain extraction (102 chunks, 121,900 vertices, 156,032
  triangles), 15 shared mesh resources / 1,083 instances, conservative culling
  (49 draws in the recorded view), normalized transforms and atlas gutters.
  Independent engine projection/unprojection/rays and game collision picking
  now have host tests, but are not wired into gameplay yet. Mesh line edges,
  dirty invalidation, proximity queries and update extraction remain pending.
- Stage 4 started: shared material-driven GL/Cg ASCII resolve, terrain material/
  shadow/elevation atlas, GPU normal lighting, real CP437/pastel output, and
  ASCII direct-color mesh shading. Host pixel checks and Vita3K native checks
  pass. Mixed-material/edge blending and complete visual parity remain pending.
- Stages 5–6 pending. Normal gameplay retains its temporary CPU implementation;
  no physical-Vita FPS improvement has been demonstrated. See
  `docs/GPU_GEOMETRY.md` and `docs/VALIDATION.md` for checks and limitations.
- Emulator setup: the removed 4154 asset is replaced by official 4168-46602447,
  authenticated against its GitHub SHA-256 and validated with native fixtures,
  600-frame parity/reset, and a fresh `make vita3k-setup` download.
- Vita3K multi-sampler rendering requires its ordinary texture cache enabled.
  The pinned build's cache-disabled path reuses one GL texture object for all
  sampler slots. Hashless caching stays disabled; a config regression test
  preserves this distinction. This is an emulator fix, not a Vita GPU workaround.
- Playable integration is now available with `--gpu-scene`: real static terrain/
  meshes, animated CP437 sprite cells, GPU depth, collision-based camera picking,
  independent HUD/menu/inventory cells, water/ripples and reflected geometry.
  Projectile damage and proximity collection were extracted from CPU rendering;
  blood material changes invalidate the GPU terrain atlas. New Game rebuilds the
  adapter before its old world is released. No normal GPU framebuffer readback.
- Shader sources now live under `shaders/` (`.glsl`, `.cg`), with shared material
  functions and CMake-tracked local include expansion. Executables embed the
  generated source; runtime shader-directory paths are not required.
- ASCII shading is split into a two-texel-per-cell style pass and a final CP437
  expansion/composition pass. Expensive material decisions no longer repeat at
  every 960x544 output pixel. Linux pixel fixtures include front/rear sprite
  depth, transparent backgrounds and independent UI. All 16 Linux tests pass,
  including an actual accelerated/offscreen GPU gameplay parity run.
- Native GXM now completes the 600-frame gameplay parity/reload run with
  `SMOKE_OK` and clean shutdown. Its GPU screenshot was inspected. The finite
  SDL emulator frontend disables host VSync to avoid Wayland frame-callback
  stalls; the recovery pass now disables host VSync in Qt too. This does not change Vita
  display synchronization or prove physical-device performance.
- Authored mesh line edges now become depth-tested GPU ribbons. Native front
  touch is polled explicitly because the dedicated GXM context does not start
  SDL video. Partial terrain-atlas uploads have actual GPU bounds/stride/
  neighbouring-texel regression checks. Physical touch remains unverified.
- Normal JS and no-JS Linux matched routes have p50 total 2.93/2.92 ms,
  respectively (AMD host, no parity harness). CPU-wall GPU stage timers expose
  collection, reflection, primary submission, UI preparation and present waits;
  they overlap scene timing and are not GPU timestamp queries.
- GPU-only cleanup: the 4,000+ line CPU rasterizer and SDL glyph-only experiment
  are removed; normal Linux/Vita launch uses GPU rendering. Gameplay unit tests
  use a non-rasterizing camera/collision view double; actual GPU gameplay and
  pixel fixtures remain. All 16 resulting host tests pass.
- Migration still has open gates: representative water/reflection and sprite
  overlap checks, update/render separation, final profiling and hardware FPS.
  Default GPU selection is not a claim that physical performance passed.
- Physical-device feedback after migration: 3–4 FPS and missing player/entities.
  Hardware acceptance is failed. Repair explicitly stores terrain depth for the
  second sprite scene, tests front/rear dynamic depth, places immutable GPU
  geometry in CDRAM, and rejects offscreen sprite images before cell iteration.
  Performance improvement and missing-entity repair need hardware confirmation.
- Native startup displays an asset-independent LOADING/progress screen before
  SDL/shader compilation. Live profiles persist every 120 frames so hardware
  diagnosis does not depend on exiting the game cleanly.
