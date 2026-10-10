# AGENTS.md

## Purpose

This repository ports Asciicker's C++17 game runtime to SDL2 on Linux and PS
Vita. Keep one shared codebase. Linux is the development and automated-test
target; Vita is the cross-compilation and packaging target. Follow
`PORTING_PLAN.md` and stop at the phase authorized by the user.

## Repository map

- `src/`: application source shared by Linux and Vita.
- `CMakeLists.txt`: targets, dependencies, compiler settings, and VPK metadata.
- `Makefile`: supported developer commands and Docker orchestration.
- `Dockerfile`: Linux and VitaSDK toolchains. Toolchains are not assumed to be
  installed on the host.
- `build/`: generated output. Never edit or commit it.
- `REQUIREMENTS.md`: product scope, acceptance criteria, and delivery phases.
- `tests/`: host-side unit and integration tests, mirroring source areas.
- `assets/`: pinned upstream maps, meshes, sprites, palette, and CP437 font.
- `third_party/`: vendored engine/QuickJS and revision/local-patch records.
- `src/app/`: scene ownership and launcher integration.
- `src/ui/`: platform-independent ASCII-cell presentation.
- `src/platform/`: SDL input mapping and engine platform hooks.
- `src/scripting/`: bounded QuickJS runtime, upstream `ak` bridge, feature probe.
- `src/audio/`: predecoded Ogg assets, bounded SPSC mixer and SDL playback.
- `docs/`: feature/system documentation and recorded validation evidence.
- `PORTING_PLAN.md`: phase checklist, decisions, and implementation progress.

Do not create these directories until code actually needs them.

## Supported operations

Run repository operations through the existing Make targets:

```sh
make linux        # Debug Linux build in the Fedora container
make run          # Build, then run the Linux executable on the host
make test         # Headless CTest tests in Docker
make test-asan    # Same host tests with AddressSanitizer
make smoke        # Host launch with scripted input checks and a BMP capture
make vita         # Release Vita build and VPK packaging in VitaSDK
make verify-vpk   # Build and verify packaged manifest entries
make vita3k-setup # Download/check pinned host emulator under build/tools/
make vita3k       # Automatically set up emulator, build/run Vita binary
make vita3k-smoke # Finite guest smoke with raw stage logs and BMP capture
make clean        # Remove generated build output
```

- Prefer adding a documented Make target over requiring contributors to
  remember a long Docker command.
- Keep CMake as the source of truth for compilation and tests; Make should
  orchestrate CMake and Docker rather than duplicate source lists or flags.
- Preserve host UID/GID handling so Docker outputs remain writable.
- Do not rely on undeclared host libraries, tools, or absolute local paths.
- Pin build inputs where practical. If an image or dependency is intentionally
  floating, record the reason and verify both targets after changing it.
- Do not commit generated binaries, CMake state, VPKs, logs, or debug screenshots.
- Use the pinned upstream SDL2 GXM build in the Vita container; do not silently
  revert to the SDK image's VitaGL SDL2 package or add shader-runtime requirements.
- Keep source acquisition out of normal CMake builds. Container setup can fetch
  checksum-pinned inputs. Record dependency updates in `third_party/README.md`.

## Change workflow

1. Understand the requirement.
2. Keep the change as small as the requirement permits.
3. Put reusable logic behind a platform-independent interface before adding
   `__vita__` branches.
4. Run `make test` for behavior changes.
5. Run `make run ARGS='--smoke --frames 240 --screenshot build/linux/smoke.bmp'`
   for UI/input/integration changes; inspect logs and the resulting framebuffer.
6. Run `make vita` whenever build files, dependencies, SDL usage, platform
   adapters, or application packaging change.
7. Update documentation and acceptance criteria when behavior or scope changes, e.g. `./docs/` folder with file per feature/core system.
8. Keep the phase checklist and validation results current. Do not mark deferred
   hardware checks or later-phase features as completed.

Before editing build or packaging settings, distinguish the native Linux target
from the Vita cross-compiled target. Never assume success on Linux proves Vita
API availability or runtime behavior.

## C++ and SDL style

- Use standard C++17 and VitaSDK-compatible standard library facilities.
  `std::span` is C++20 and must not be used with this project's C++17 baseline.
- Follow the existing style: four-space indentation, braces on the same line,
  `kPascalCase` constants, `PascalCase` types, and `snake_case` local variables
  and functions.
- Prefer small, single-purpose translation units and explicit ownership.
- Use RAII for SDL resources and other acquired handles once code is factored
  beyond a short setup scope. Avoid scattered cleanup paths.
- Prefer `constexpr`, scoped enums, `std::array`/`std::vector`, and value types.
  Avoid macros except for platform or build integration.
- Keep warnings clean. Do not silence a warning without documenting why.
- Report SDL/platform failures with enough context and `SDL_GetError()` where
  applicable; recover when the product requirements call for it.
- Use Vita's native 960x544 coordinate space as the UI design baseline. Scale
  or letterbox intentionally on desktop rather than maintaining unrelated UIs.
- Keep platform conditionals at adapter boundaries. Cell conversion, input
  mapping, and scene integration should be host-testable. Keep upstream code
  recognizable; document each necessary local engine patch.
- Preserve upstream's custom controller mapping; SDL button indices are not
  interchangeable with engine button indices.
- Keep diagnostics bounded during unlimited sessions. Report wall-clock timings
  separately from the synthetic simulation time used by smoke tests.
- Engine and presenter hot loops retain `-O2` with Debug symbols. Do not judge
  raster performance from an accidentally unoptimized Debug build. Preserve
  exact coverage blending and unaligned/padded output tests when optimizing.
- Use the documented OpenGL/no-hashless-cache Vita3K baseline. Its host backend
  is independent of Vita's SDL GXM renderer. Check guest `SMOKE_OK` and clean
  shutdown in the latest stage-log section, not only emulator process status.
- Interactive Vita3K uses Qt without `--console` (console skips its audio/pad
  initialization). SDL is for synthetic smoke only in the pinned build: smoke
  success does not verify host keyboard/mouse forwarding. Test human input at
  the frontend boundary when changing emulator launch settings.
- Offline gameplay, audio and QuickJS bindings are now in scope. Networking,
  editor, campaign persistence and a replacement GPU renderer are not.
- Keep the official QuickJS 2026-06-04 snapshot pinned on both targets. Preserve
  shared-buffer offsets, UTF-8 boundaries, script limits, and callback teardown.
  Run scripts only on the game thread; do not expose host filesystem or shells.
- Audio callbacks must not allocate, log, read files, or take locks. Decode
  samples before unpausing the device; close/join it before resetting mixer data.
  Keep the command queue bounded and report dropped commands outside callbacks.
- Keep settings under the writable profile root. Reject partial/corrupt files
  without partially applying them; never overwrite real preferences in tests.


## Tests

New behavior is not complete without tests at the lowest useful layer.

- Use CMake/CTest through `make test`. Tests run in Docker without a display,
  audio device, Vita, network, or user input after dependencies are installed.
- Unit-test palette/glyph conversion, row orientation/pitch, buffer bounds,
  controller mappings/deadzones, and asset-path/manifest behavior.
- Exercise real map loading, rendering, input transitions, physics, and repeated
  scene teardown. Keep tests of observable behavior rather than implementation
  copies. Control timestamps and use tolerances for cross-architecture numerics.
- Assert numerical results with explicit tolerances based on requirements; do
  not compare floating-point values for exact equality.
- Keep benchmarks separate from correctness tests. Use the 120x68 cell scene
  baseline and report simulation/render, glyph, upload, and present stages.
- UI/input changes require a Linux host launch and log/framebuffer inspection.
  For this implementation pass, Linux checks replace Vita runtime tests at the
  user's request. Keep Vita compilation/VPK checks; Vita3K is supplementary.
  Never infer hardware performance or suspend/resume correctness from Linux.
- `src/app/self_test.h` is a diagnostic parity harness shared by host tests and
  the actual Vita executable (`--smoke --frames 600 --verify-gameplay`). It
  intentionally mutates gameplay and injects failing scripts: never run it
  during normal play or use its error counts/timings as a gameplay benchmark.
- Test observable combat damage/projectiles, inventory ownership and controller
  actions, menu Continue/New Game, console execution, `ak` callbacks/bounds/
  Unicode/limits, mixer cues/clipping/mute/overflow, audio fallback and pause.
  Repeated scene loads must reset world, inventory, callbacks and script state.
- Compare default JS against `--no-js` using matched runs without the parity
  harness. Include `js_p50/js_p95`; scene timing includes JavaScript, so those
  timings overlap and must not be summed. Record backend and host contention.
- A bug fix should include a regression test when the failure can be reproduced
  below the hardware/UI layer.

Record commands, outcomes, and limitations in `docs/VALIDATION.md`. Generated
logs/screenshots remain under `build/` or `/tmp`, not in source control.

## Definition of done

- The implemented behavior maps to a requirement and acceptance criterion.
- Relevant host tests pass, and the Linux application still builds.
- The Vita VPK builds when the change can affect cross-compilation or runtime.
- No generated artifacts are committed.
- Errors and degraded states are visible to the user where appropriate.
- Documentation, controls, and build commands match the delivered behavior.

