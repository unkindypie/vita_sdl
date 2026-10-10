# Vendored dependencies

These are checked-in snapshots. Application builds never download engine or
JavaScript sources. Update deliberately and rerun Linux tests and both builds.

The optional host emulator is not vendored: `make vita3k-setup` downloads the
checksum-pinned official Vita3K 4154-b75d5e62 Linux x86_64 release into ignored
`build/tools/`. Its asset ID/hash and host requirements are documented in
[`docs/VITA3K.md`](../docs/VITA3K.md); update the pin only after a guest smoke.
The official archive retains its upstream license and resources.

## Asciicker

- Origin: https://github.com/msokalski/asciicker
- Revision: `8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635`
- Selected game runtime sources/headers in `asciicker/`; assets in `../assets/`.
- Upstream MIT notice: `asciicker/LICENSE`. Embedded uPNG/tinfl notices remain
  in their source files. Asset-specific attribution review is still required
  before a public release; this is a development preview.
- Local patches: `game.cpp` routes menu exit through portable cleanup, refreshes
  item prototype sprite pointers on reload, and delegates validated settings to
  `src/platform/config.cpp`. Portable menus hide unsupported fixed-grid zoom and
  Vita fullscreen, expose useful controls/credits, resume the loaded world and
  request safe New Game teardown/reload instead of upstream's editor loader.
  Main-menu exit cancellation uses its own stack, not the game's stack.
  `game_api.cpp` guards absent callbacks and fixes onItem array/null detection
  and the story-id/description output layout (story at byte 4, text at byte 8),
  plus terminates bounded player names. `stb_vorbis.cpp` includes Vita alloca.h.
- Excluded from compilation: desktop launcher/editor, GL/terminal/X11 backends,
  V8 bindings, network/server implementations, and desktop-specific audio.

## QuickJS

- Origin/archive: https://bellard.org/quickjs/quickjs-2026-06-04.tar.xz
- Runtime: **2026-06-04**, selected per the user's preference and verified on
  the official release page. SHA-256:
  `b376e839b322978313d929fd20663b11ba58b75df5a46c126dd19ea2fa70ad2a`.
- Core runtime with current dtoa, replacing the earlier QuickJS Vita fork and
  obsolete libbf. MIT notice: `quickjs/LICENSE`. No quickjs-libc, shell, worker,
  filesystem/module loader, precompiled bytecode or C++ wrapper dependencies.
- Local patches in `quickjs.c`: `PORT_NO_ATOMICS` excludes unsupported shared
  atomics on both targets for a consistent single-threaded embedded API;
  Vita includes malloc.h for newlib allocation accounting; Vita dates use UTC
  because newlib's struct tm lacks tm_gmtoff. Shared **ordinary** ArrayBuffer
  views and all game bindings still work. See `docs/SCRIPTING.md`.
- Same sources and bootstrap run on Linux and actual ARM code in Vita3K;
  evaluation, game callbacks, typed arrays, Unicode, bounds, interruption,
  stack/memory limits and repeated teardown are exercised, not just linked.

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
