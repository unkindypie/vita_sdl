# AGENTS.md

## Purpose

This repository is a C++17/SDL2 starter for a PS Vita application that must
also run on Linux for fast development and testing. Keep one shared codebase
where practical. Linux is the primary development and automated-test target;
the Vita build is the compatibility and packaging target.

## Repository map

- `src/`: application source shared by Linux and Vita.
- `CMakeLists.txt`: targets, dependencies, compiler settings, and VPK metadata.
- `Makefile`: supported developer commands and Docker orchestration.
- `Dockerfile`: Linux and VitaSDK toolchains. Toolchains are not assumed to be
  installed on the host.
- `build/`: generated output. Never edit or commit it.
- `REQUIREMENTS.md`: product scope, acceptance criteria, and delivery phases.

As the project grows, prefer these boundaries:

- `src/audio/`: capture adapters, buffering, and audio-domain types.
- `src/pitch/`: platform-independent pitch estimation and smoothing.
- `src/ui/`: SDL rendering and interaction.
- `src/platform/`: unavoidable Linux/Vita-specific integration.
- `tests/`: host-side unit and integration tests, mirroring source areas.

Do not create these directories until code actually needs them.

## Supported operations

Run repository operations through the existing Make targets:

```sh
make linux        # Debug Linux build in the Fedora container
make run          # Build, then run the Linux executable on the host
make vita         # Release Vita build and VPK packaging in VitaSDK
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
- Do not commit generated binaries, CMake state, VPKs, or captured audio.

## Change workflow

1. Understand the requirement.
2. Keep the change as small as the requirement permits.
3. Put reusable logic behind a platform-independent interface before adding
   `__vita__` branches.
4. Run tests
5. Build and run test on Linux during development.
6. Run `make vita` whenever build files, dependencies, SDL usage, platform
   adapters, or application packaging change.
7. Update documentation and acceptance criteria when behavior or scope changes, e.g. `./docs/` folder with file per feature/core system.

Before editing build or packaging settings, distinguish the native Linux target
from the Vita cross-compiled target. Never assume success on Linux proves Vita
API availability or runtime behavior.

## C++ and SDL style

- Use standard C++17 and the standard library unless VitaSDK compatibility has
  been confirmed.
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
- Keep platform conditionals at adapter boundaries. Shared pitch, smoothing,
  history, and graph logic must not depend on SDL rendering or Vita headers.

## Real-time audio rules

- Treat the audio capture callback as real-time code: do not allocate memory,
  perform file I/O, log, render, or take contended locks in it.
- Move samples out of the callback through a preallocated bounded buffer. Make
  overflow behavior explicit and observable outside the callback.
- Carry sample rate, channel count, sample format, frame count, and timestamps
  explicitly; do not bury them in global state.
- Convert input to one documented internal mono sample representation before
  pitch analysis.
- Keep capture, pitch estimation, smoothing, and rendering independently
  replaceable. Hardware capture must not be required to test DSP logic.
- Bound CPU use, memory, and history length with Vita hardware as the limiting
  target. Avoid per-frame and per-sample heap churn.
- Never persist or transmit microphone audio unless a later requirement
  explicitly adds it and includes clear user consent and retention behavior.

## Tests

New behavior is not complete without tests at the lowest useful layer.

- Add host-side tests through CMake/CTest and expose them through a `make test`
  target when the first test is introduced. Tests must run in Docker and must
  not require a display, microphone, Vita, network, or user input.
- Unit-test pure logic: PCM conversion, buffering, windowing, pitch estimation,
  confidence gating, smoothing, note/cents conversion, history sampling, and
  graph coordinate transforms.
- Use deterministic generated signals and small checked-in fixtures. Cover
  silence, noise, clipped input, frequencies near supported boundaries,
  octave-error cases, buffer wraparound, and discontinuous timestamps.
- Assert numerical results with explicit tolerances based on requirements; do
  not compare floating-point values for exact equality.
- Keep algorithm benchmarks separate from unit tests, but track processing
  time and allocations on representative Vita-sized frames.
- Use an injectable fake capture source for pipeline/integration tests.
- UI changes need a short manual Linux check. Audio or input changes also need
  a manual Vita check on real hardware; document the device/firmware and result
  in the change summary.
- A bug fix should include a regression test when the failure can be reproduced
  below the hardware/UI layer.

Until test infrastructure exists, the minimum verification is `make linux` and
a manual `make run` smoke test. Do not describe that as unit-test coverage.

## Definition of done

- The implemented behavior maps to a requirement and acceptance criterion.
- Relevant host tests pass, and the Linux application still builds.
- The Vita VPK builds when the change can affect cross-compilation or runtime.
- No generated artifacts or recordings are committed.
- Errors and degraded states are visible to the user where appropriate.
- Documentation, controls, and build commands match the delivered behavior.

