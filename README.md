# Asciicker Vita / Linux

A shared C++17/SDL2 port of Asciicker's offline game runtime. Version 0.3
loads the real upstream map and renders its terrain, sprites, and characters
through a dedicated OpenGL (Linux) / native GXM (Vita) GPU ASCII renderer.
It supports movement, camera, collision, combat, inventory, menus, SDL audio,
and the upstream JavaScript `ak` API through QuickJS **2026-06-04**. Offline
parity is exercised on Linux and in the ARM/Vita binary through Vita3K. This
is a port of the upstream sandbox, not a new campaign or multiplayer game.

The physical-Vita GPU rewrite and modular engine extraction are tracked in
[`GPU_REWRITE_PLAN.md`](GPU_REWRITE_PLAN.md). Stage 1 provides rendering
contracts/dispatch; Stage 2 adds independent OpenGL/GXM geometry and display
ownership. The real-world GPU diagnostic uploads all 4,876 patches and 15 mesh
resources once (1,083 shared mesh instances), with GPU lighting and a
material-driven CP437/pastel ASCII resolve. It also checks real material/font
lookup pixels on both backends. Playable GPU integration preserves offline
gameplay, sprites, water, independent UI and New Game. See
[`GPU geometry diagnostics`](docs/GPU_GEOMETRY.md). The explicit
`--probe-gpu` diagnostic checks the separately installed Vita shader compiler;
see [`engine boundaries`](docs/ENGINE_ARCHITECTURE.md). Native gameplay requires
`ur0:data/libshacccg.suprx`; it is not bundled. No hardware FPS improvement is
claimed until measured on a real Vita.

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
make vita3k-bench # finite uncapped emulator diagnostic, normal JS/audio
make verify-vpk  # build Vita and check packaged manifest entries
make vita3k-setup # download/check pinned emulator (Linux x86_64)
make vita3k-smoke # automatic emulator setup, finite smoke of ARM/Vita build
make clean       # remove generated build output
```

For manual emulator play, run `make vita3k` and focus the game window. WASD moves,
Q/E rotates, X attacks, C jumps, Enter opens inventory, and Right Shift opens
the menu. Mouse input emulates Vita touch. See [emulator controls and timed
performance checks](docs/VITA3K.md#interactive-play-and-manual-performance-checks).

GPU scene rendering is the sole normal path; the old CPU rasterizer and SDL
glyph-only experiment were removed. See [GPU rendering](docs/GPU_RENDERING.md).

Non-destructive fresh build checks:

```sh
make test LINUX_BUILD_DIR=build/linux-fresh CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release
make verify-vpk VITA_BUILD_DIR=build/vita-fresh
```

Additional diagnostic runs:

```sh
make run ARGS='--smoke --frames 240 --screenshot build/linux/smoke.bmp'
make run ARGS='--probe-js'
make run ARGS='--smoke --frames 600 --verify-gameplay'
make run ARGS='--script assets/scripts/startup.js'
make run ARGS='--smoke --seconds 900'
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIODRIVER=dummy build/linux/vita_sdl2 --smoke --frames 240
```

Use `--assets DIR` and `--manifest FILE` to override assets. Default Linux paths
are relative to the executable, so launching from another working directory
works. Linux settings use SDL's user preference directory. Vita uses `app0:assets`
for bundled data and `ux0:data/asciicker/` for configuration and logs.

The Vita container explicitly builds upstream SDL2 2.32.10 with its native GXM
backend instead of the SDK image's bundled VitaGL SDL2 variant. There is no
VitaGL dependency. The dedicated GXM renderer does require the separately
installed Sony shader compiler module above. Install the VPK through VitaShell or
another homebrew installer. Runtime behavior on real Vita remains unverified in
this implementation pass; Linux validation substitutes were requested.
Vita3K smoke passes using the documented OpenGL/no-hashless-cache configuration;
see [emulator setup and commands](docs/VITA3K.md). Emulator timings are not
real-device performance measurements.

## Controls

Move with WASD/arrows, left stick, or D-pad. Rotate with Q/E, L/R shoulders, or
right-stick X. Jump with Space or Circle/SDL B. Exit with Ctrl+Q/window close
or the Exit menu. F1/Select opens the menu; I/Start opens
inventory. Tab opens chat; a leading backslash evaluates JavaScript. On Vita,
Select+Start opens the controller text keyboard. See [controls](docs/CONTROLS.md).

## Development documents

- [Porting plan and progress](PORTING_PLAN.md)
- [Current requirements](REQUIREMENTS.md)
- [Contributor/agent operations](AGENTS.md)
- [Rendering and diagnostics](docs/RENDERING.md)
- [Validation results](docs/VALIDATION.md)
- [Vita3K setup and smoke checks](docs/VITA3K.md)
- [Dependency revisions and local patches](third_party/README.md)
- [Offline feature parity and limitations](docs/PARITY.md)
- [JavaScript embedding and console](docs/SCRIPTING.md)
- [Audio playback and failure behavior](docs/AUDIO.md)

Upstream: https://github.com/msokalski/asciicker. Preserve its MIT notice and
embedded third-party notices when distributing source/binaries. The VPK contains
a user-supplied photo icon and license/provenance notices. Real-device acceptance and
independent asset-rights clearance are not established by emulator tests.

## Playable GPU migration

The native-GXM/OpenGL scene renderer launches with `make run`.
Shader sources live under `shaders/` as `.glsl`
and `.cg` files. See [GPU rendering](docs/GPU_RENDERING.md) and
[the rewrite plan](GPU_REWRITE_PLAN.md) for prerequisites and remaining gates.
