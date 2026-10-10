# Asciicker Vita / Linux Preview

A shared C++17/SDL2 port of Asciicker's game runtime. The current Phase 2 preview
loads the real upstream map and renders its terrain, sprites, and characters
through the original CPU ASCII renderer with an SDL2 streaming-texture presenter.
It supports movement, camera, collision, and jump. Full game scripting, audio,
networking, editor, and gameplay parity are deferred.

## Build and run

Docker is required for compilation. No host C++ or Vita toolchain is required.
To launch on Linux, install the SDL2 runtime and use a desktop session:
`sudo dnf install SDL2` on Fedora. Container builds require network access on
first setup; engine and QuickJS source snapshots are vendored.

```sh
make linux       # build/linux/vita_sdl2
make run         # build in Docker, launch on the Linux host
make test        # headless CTest unit/integration/launch checks in Docker
make test-asan   # repeat host tests with AddressSanitizer
make smoke       # desktop smoke, movement/camera checks, build/linux/smoke.bmp
make vita        # build/vita/vita_sdl2.vpk
make verify-vpk  # build Vita and check packaged manifest entries
make vita3k-smoke VITA3K=/path/to/Vita3K # finite smoke of ARM/Vita build
make clean       # remove generated build output
```

Additional diagnostic runs:

```sh
make run ARGS='--smoke --frames 240 --screenshot build/linux/smoke.bmp'
make run ARGS='--probe-js'
make run ARGS='--smoke --seconds 900'
SDL_VIDEODRIVER=dummy build/linux/vita_sdl2 --smoke --frames 240
```

Use `--assets DIR` and `--manifest FILE` to override assets. Default Linux paths
are relative to the executable, so launching from another working directory
works. Linux settings use SDL's user preference directory. Vita uses `app0:assets`
for bundled data and `ux0:data/asciicker/` for configuration and logs.

The Vita container explicitly builds upstream SDL2 2.32.10 with its native GXM
backend instead of the SDK image's bundled VitaGL SDL2 variant. There is no
runtime VitaGL shader-compiler dependency. Install the VPK through VitaShell or
another homebrew installer. Runtime behavior on real Vita remains unverified in
this implementation pass; Linux validation substitutes were requested.
Vita3K smoke passes using the documented OpenGL/no-hashless-cache configuration;
see [emulator setup and commands](docs/VITA3K.md). Emulator timings are not
real-device performance measurements.

## Controls

Move with WASD/arrows, left stick, or D-pad. Rotate with Q/E, L/R shoulders, or
right-stick X. Jump with Space or Circle/SDL B. Exit with Escape/window close or
Select/SDL Back. Start exposes upstream inventory behavior, whose full parity
is deferred. See [controls](docs/CONTROLS.md).

## Development documents

- [Porting plan and progress](PORTING_PLAN.md)
- [Current requirements](REQUIREMENTS.md)
- [Contributor/agent operations](AGENTS.md)
- [Rendering and diagnostics](docs/RENDERING.md)
- [Validation results](docs/VALIDATION.md)
- [Vita3K setup and smoke checks](docs/VITA3K.md)
- [Dependency revisions and local patches](third_party/README.md)

Upstream: https://github.com/msokalski/asciicker. Preserve its MIT notice and
embedded third-party notices when distributing source/binaries. Asset attribution
review and final app identity are required before a public release.
