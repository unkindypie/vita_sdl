# Asciicker Linux / PS Vita requirements

Status: offline runtime version 0.3, 2026-10-10. All offline-port phases are now
authorized, superseding the Phase 2 stopping point and voice-training direction.
See `PORTING_PLAN.md` and `docs/PARITY.md`.

GPU rewrite authorized 2026-10-11 after the user's physical-Vita 2–3 FPS report.
Active staged execution is tracked in `GPU_REWRITE_PLAN.md`. The additional
on-device shader compiler prerequisite is authorized; hardware acceptance is
now essential for the performance objective, not replaceable by emulator FPS.

## Required behavior

- **PORT-001:** Shared C++17 runtime through Docker/Make/CMake; separate Linux
  and Vita cross-compiled output, pinned source/toolchain inputs.
- **PORT-002:** Preserve physics, terrain/world formats, sprites and ASCII-cell
  semantics rather than substituting a screenshot. Maintain a single GPU scene
  implementation; no legacy CPU rasterizer or hidden fallback.
- **PORT-003:** Load real `game_map_y8.a3d`, meshes and sprites.
- **PORT-004:** 120x68 cells, upstream 8x8 CP437 font/palette, 960x544 baseline;
  intentional desktop letterboxing and readable state/controls.
- **PORT-005:** Keyboard/Vita movement, camera, collision, jump, combat, inventory
  pickup/equip/drop, menus and exit, without L2/R2/stick-click dependency.
- **PORT-006:** Asset preflight reports missing files and exits nonzero.
- **PORT-007:** VPK contains all manifest assets, icon, metadata and notices;
  writable settings/logs never target bundled `app0:` assets.
- **PORT-008:** Clean teardown/reload, input reset on focus/disconnect, audio
  pause/resume; New Game resets world, inventory, JS context and callbacks.
- **PORT-009:** Explicit offline services. Editor, multiplayer, campaign saves,
  and new content remain excluded. GPU migration is now authorized separately;
  its current experiment/remaining work is documented in `docs/GPU_RENDERING.md`.
- **PORT-010:** Same official QuickJS 2026-06-04 on both targets. Preserve the
  synchronous `ak` API/bootstrap, shared-buffer layout, callbacks, UTF-8 and
  console. Bound memory/stack/execution and recover after errors.
- **PORT-011:** Bounded diagnostic windows, scene/glyph/upload/present/JS timing,
  memory snapshots, finite smoke and framebuffer capture. JS-off is labelled
  as a performance baseline, not the default game mode.
- **PORT-012:** Headless Docker CTest tests require no display/audio hardware,
  Vita, network or input after setup. Include regression and ASan checks.
- **PORT-013:** SDL audio plays real ambience/footstep cues through bounded,
  allocation-free callback mixing. Mute persists; device failures visibly
  degrade to silent gameplay. No microphone/recording dependency.
- **PORT-014:** Usable main/in-game menus, validated settings and talk presets.
  Continue resumes the current world, not a nonexistent upstream campaign save.
- **PORT-015:** Explicit combat/inventory/menu/audio/script evidence on Linux
  and actual ARM code in Vita3K, not inferred from linking or one screenshot.
- **GPU-001:** Native GPU geometry/depth/lighting/material/ASCII/water/sprite
  rendering, not only GPU glyph expansion. CPU simulation/UI remain independent.
- **GPU-002:** Reusable engine contracts independent of game rules, SDL and
  Vita headers; game-specific world formats and rules live behind adapters.
- **GPU-003:** Render once per snapshot without advancing simulation/scripts;
  collision/picking/proximity must not require GPU scene/depth readback.
- **GPU-004:** Probe/document shader prerequisites; fail visibly on missing
  compiler or shader/pass errors. No bundled proprietary compiler module.
- **GPU-005:** Preserve offline gameplay parity and the material-driven ASCII
  style/pastel grading. Explicitly record intentional visual differences.
- **GPU-006:** Target sustained 60 FPS on real Vita, JS/audio enabled, matched
  representative routes and bounded memory; report median/p95 and device clocks.
  Keep this criterion pending until measured on hardware.

## Acceptance and evidence

- `make test`, `make test-asan`, `make linux`, `make vita`, `make verify-vpk`.
- `make run ARGS='--smoke --frames 600 --verify-gameplay --screenshot build/linux/parity.bmp'`:
  movement/camera/grounded, New Game, parity and clean shutdown markers.
- `make vita3k-smoke VITA3K=/path/to/Vita3K`: same guest checks; inspect latest
  `stages.log` boot section and framebuffer, not only host exit status.
- JS-on/JS-off measurements without the intrusive parity harness, plus a
  15-minute Linux JS/audio soak recording actual RSS/timing observations.
- `docs/PARITY.md` distinguishes delivered behavior, upstream absences and
  hardware-only acceptance; `docs/VALIDATION.md` records commands/results.

Linux and emulator checks substitute for device tests as requested. They do
not establish physical controls/speakers/suspend behavior or stable 60 FPS /
actual memory budget on Vita. Hardware certification and independent upstream
asset-rights clearance remain release gates, not silently passed tasks.
