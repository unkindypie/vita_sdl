# Asciicker Linux / PS Vita Preview Requirements

Status: Phase 0–2 implementation baseline, 2026-10-10.
See `PORTING_PLAN.md` for tasks, decisions, progress, and deferred phases.
The previous voice-training product direction has been superseded.

## Required behavior through Phase 2

- **PORT-001:** Build the same C++17 game runtime for Linux and PS Vita through
  Docker, Make, and CMake, with separate native/cross-compilation directories.
- **PORT-002:** Use pinned vendored Asciicker sources and preserve its CPU scene
  renderer, terrain/world formats, sprites, physics, and ASCII-cell semantics.
- **PORT-003:** Load the upstream `game_map_y8.a3d` and required meshes/sprites;
  display a real interactive scene rather than a canned image.
- **PORT-004:** Present a 120x68 cell grid using the upstream 8x8 CP437 atlas and
  palette at 960x544, with predictable desktop resizing and readable hints.
- **PORT-005:** Support character movement, camera rotation, jumping, and exit
  with keyboard and Vita-style controller controls. Keep terrain collision.
- **PORT-006:** Validate bundled asset availability before engine initialization
  and report missing files with a nonzero process exit.
- **PORT-007:** Package the complete required asset manifest and dependency
  license notices in the VPK. Keep configuration/logs out of the asset root.
- **PORT-008:** Close acquired resources on exit; pause/reset input on focus or
  lifecycle interruption and support controller connection/disconnection.
- **PORT-009:** Keep script/audio/network states explicit. The preview operates
  offline, without full game scripting or audio playback.
- **PORT-010:** Compile a standalone QuickJS probe on both platforms; on Linux
  verify source evaluation, native functions, typed-array writes through a
  shared ArrayBuffer, exceptions, and runtime teardown. Full game bindings wait
  for Phase 3.
- **PORT-011:** Bound frame/history diagnostics and provide per-stage frame-time
  logs, finite smoke runs, and framebuffer capture for diagnosis.
- **PORT-012:** Keep headless CTest tests runnable through `make test` without
  microphone, display, network, Vita, or user input after build setup.

## Acceptance for this implementation pass

- `make linux`, `make test`, and `make vita` succeed.
- `make run` with smoke options launches on the Linux desktop, displays the
  actual scene, logs movement/camera/grounded checks, and exits cleanly.
- Presenter tests check palette, glyph placement, row inversion, blank cells,
  output bounds, and padded rows. Scene tests load/render/teardown repeatedly.
- A 15-minute Linux runtime soak records memory and timing without crashing or
  hanging. Memory observations are reported rather than inferred from bounds.
- VPK contents match the asset manifest.
- Vita runtime tests are replaced by Linux tests at the user's request.
  Vita3K may provide additional smoke evidence; actual device performance,
  suspend/resume, and controls remain unverified until tested on hardware.
- The desired Vita baseline is 30 FPS; no hardware frame-rate claim is made
  from desktop or emulator timings.

## Deferred beyond Phase 2

Full QuickJS game bindings, script console/text entry, audio playback integration,
combat/inventory/menu/save parity, networking, editor port, GPU glyph presenter,
GPU scene renderer, and public release readiness are deferred. Existing upstream
actions may be reachable, but their presence does not establish parity.
