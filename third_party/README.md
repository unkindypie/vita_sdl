# Vendored dependencies

These are checked-in snapshots. Application builds never download engine or
JavaScript sources. Update deliberately and rerun Linux tests and both builds.

## Asciicker

- Origin: https://github.com/msokalski/asciicker
- Revision: `8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635`
- Selected game runtime sources/headers in `asciicker/`; assets in `../assets/`.
- Upstream MIT notice: `asciicker/LICENSE`. Embedded uPNG/tinfl notices remain
  in their source files. Asset-specific attribution review is still required
  before a public release; this is a development preview.
- Local patches: `game.cpp` routes menu exit through the portable shutdown hook
  under `PORTABLE_SDL`; the item prototype table refreshes its sprite pointers
  on each load so repeated scene teardown/reload does not use freed sprites.
- Excluded from compilation: desktop launcher/editor, GL/terminal/X11 backends,
  V8 bindings, network/server implementations, and audio playback.

## QuickJS Vita

- Origin: https://github.com/MrProcastinator/quickjs-vita
- Revision: `2fe9a61a6ef718caabffff9f0e6385fc8f5d8c1e`
- Runtime version: see `quickjs/VERSION`; MIT notice: `quickjs/LICENSE`.
- Core runtime only, including libbf; no quickjs-libc, shell, filesystem helpers,
  precompiled script toolchain, or C++ wrapper dependency.
- Local patches in `quickjs.c`: `PORT_NO_STDIO_REDIRECT` lets the application own
  logging; a const-correct allocator wrapper supports modern VitaSDK newlib.
- Built on both platforms and used only by the Phase 0 compatibility probe.
  The game scripting hooks remain explicitly disabled through Phase 2.
- The port README's precompiled-script limitation concerns its supplied tooling;
  our Linux core-runtime probe evaluates source successfully. Vita source
  evaluation is cross-compiled but remains runtime-unverified unless an emulator
  or hardware probe is explicitly recorded.

## SDL2 / toolchains

- Linux: Fedora 42 container with SDL2-devel; image digest pinned in Dockerfile.
- Vita: pinned VitaSDK image plus upstream SDL2 **2.32.10**, built using native
  GXM, with PVR/PIB disabled. Archive SHA-256 is pinned in Dockerfile.
- Vita SDL is installed at `/opt/sdl2-vita` inside the container and selected by
  `make vita`; the SDK image's bundled vitaGL variant is intentionally unused.
- Building the container requires network access to obtain its inputs. Normal
  CMake application compilation uses only installed/vendored dependencies.
- Image digests/archive hash pin source inputs, not every Fedora package
  revision: Linux dnf packages still come from the Fedora repositories.
