# Running the Vita build in Vita3K

Vita3K is a host debugging tool, not an application dependency. On Linux x86_64,
the project downloads the tested official binary automatically, verifies its
SHA-256, and extracts it to ignored `build/tools/vita3k/`. No `/tmp` installation,
source clone, or PATH change is needed. These commands build the VPK, generate
a known-working config, and install/run it:

Each selected profile must separately contain
`storage/ur0/data/libshacccg.suprx` beneath its `VITA3K_DIR`. A fresh profile does
not inherit this compiler from another profile. The launcher checks before
starting the emulator and prints the exact required path when missing. The
module is not bundled/downloaded by setup; use your separately installed copy.

```sh
make vita3k-setup # optional: download/check without compiling the game
make vita3k
make vita3k-smoke
# Optional fresh, isolated install (does not remove existing emulator data):
make vita3k-smoke VITA3K_DIR=build/vita3k-fresh
```

## Interactive play and manual performance checks

`make vita3k` uses the **Qt frontend**. Click the **Asciicker Vita game window**
(not the emulator library window) to focus it. Keyboard input emulates a Vita
controller; it is not the native Linux application's keyboard layout:

| Action | Host key |
| --- | --- |
| Move | WASD or arrow keys |
| Camera | Q/E or J/L |
| Attack / confirm | X (Vita Cross) |
| Jump | C (Vita Circle) |
| Inventory | Enter (Vita Start) |
| Equip/use | Z (Vita Square) |
| Pick up | V (Vita Triangle) |
| Menu | Right Shift (Vita Select) |
| Navigate menus/inventory | Arrow keys |
| Controller text keyboard | Hold Right Shift, press Enter |
| Fullscreen | F11 |

Left mouse press/drag/release emulates front-screen touch, including the engine's
touch UI; it is not a desktop mouse-look control. T switches front/rear touch:
leave it on front touch for this app. Use the in-game Exit confirmation to end
a session cleanly and flush the final performance summary.

Normal launches have JavaScript/audio enabled and no synthetic controls or parity
harness. For a timed session (ordinary real-time simulation, manual input):

```sh
make vita3k VITA3K_ARGS='--seconds, 120'
# Matched JavaScript-disabled comparison:
make vita3k VITA3K_ARGS='--seconds, 120, --no-js'
```

Inspect the latest session's `PROFILE` and `SCRIPT_PROFILE` entries in the stage
log below. Match camera/route/settings and minimize host contention. Qt frontend
timings are a separate baseline from historical SDL smoke results; neither
measures real Vita hardware performance.

Host VSync defaults OFF for both frontends. Guest VBlank still paces normal
play at 60 Hz. The pinned Qt/OpenGL frontend with host VSync ON can wait for
multiple host refreshes inside offscreen GXM completion calls, reducing guest
FPS even when collection and JavaScript are inexpensive. Override with
`VITA3K_HOST_VSYNC=ON` only for explicit comparisons.

For uncapped emulator headroom (not hardware FPS), use `make vita3k-bench`.
It runs a finite 3600-frame route with normal JS/audio, disables guest display
pacing, and does not inject parity scripts or request GPU captures. Set
`VITA3K_BENCH_FRAMES=1200` for a shorter run. `--unpaced` requires a finite
frame/time limit and is rejected with the intrusive gameplay parity harness.
Use ordinary `make vita3k` for manual play, not this benchmark mode.

`make vita3k-smoke` deliberately uses the SDL frontend with synthetic in-game
input. In pinned build 4154, that frontend does **not** forward host keyboard
gameplay input or mouse clicks; passing that smoke does not validate human input.
Do not add `--console` to Qt launches: this emulator version skips SDL audio/pad
initialization in console mode and crashes when the game opens an audio port.

Setup requires host `curl`, `unzip`, `sha256sum` (coreutils), and `ldd`.
The official binary also requires desktop libraries even with the SDL frontend:

```sh
# Fedora (the validated host family):
sudo dnf install curl unzip coreutils glibc-utils qt6-qtbase qt6-qtsvg qt6-qtmultimedia libSM libICE libX11 libXext libxkbcommon wayland mesa-libGL mesa-libEGL openssl-libs brotli zstd-libs
# Ubuntu 24.04 / Debian-family starting point:
sudo apt install curl unzip coreutils libc-bin libqt6widgets6 libqt6svg6 libqt6multimedia6 libqt6network6 libqt6concurrent6 libsm6 libice6 libx11-6 libxext6 libxkbcommon0 libwayland-client0 libgl1 libegl1 libssl3 libbrotli1 libzstd1
```

Setup checks linked libraries before the expensive VPK build and reports missing
ones. A working desktop/GPU driver and sufficiently recent glibc are still
required; arbitrary distributions and architectures are not guaranteed. Other
emulator installations can opt out of download with `VITA3K=/path/to/Vita3K`.
No system packages, firmware, or commercial games are automatically installed.
The project config sets `warn-missing-firmware: false` to suppress the repeated
Qt firmware/font-package popup for this homebrew. This only hides the warning;
it does not install those packages or make other games firmware-independent.

The pin is official build **4168-46602447**, release asset **630853783**,
SHA-256 `ad62f00603cfcfa2412077a5280510b4f0e450583b592191225717bf3cca0c74`.
Validated with native GXM world/material/composition/atlas-update fixtures and
600-frame gameplay parity/reset. Official continuous assets can be removed by
upstream; if this pin disappears, update ID and hash together after validation,
or use an existing checked binary through `VITA3K`. Never silently fetch latest.
`tools/setup_vita3k.sh` uses the asset ID rather than the mutable `continuous`
download name. If upstream removes that asset, setup fails explicitly rather
than silently taking a different build. Update ID/checksum together and rerun
the guest smoke before adopting a new version. Successful installs are reused
offline; interrupted downloads are not treated as installed emulators.
`make clean` removes the downloaded emulator as well as generated storage.

The smoke runs the actual ARM Vita binary for 600 frames with synthetic movement
and camera events, a world/runtime reload and the gameplay parity harness.
It should append `NEW_GAME_OK`, `GAMEPLAY_CHECKS_OK`, `SMOKE_OK` with all three checks set to 1,
then `SHUTDOWN_OK`, to:

`build/vita3k/storage/ux0/data/asciicker/stages.log`

The framebuffer capture is alongside it as `vita3k.bmp`. This is an explicit GPU
display-target readback, not a capture of the emulator window. The stage log also
records successful presentation and progress. Logs append across launches;
inspect the latest boot section. Emulator exit status alone is not proof of a
successful guest smoke. An early exit or `SMOKE_FAIL` needs investigation.

`make vita3k VITA3K_ARGS='--smoke, --seconds, 60'` supports longer checks.
Vita3K guest arguments must be comma-and-space separated. Select opens the menu;
use its Exit confirmation to leave normal play. `make clean` also removes this generated emulator storage, including its
installed preview, settings, screenshots, and logs; it does not affect your
normal Vita3K storage.

## Working graphics settings

Validated with Linux Vita3K **0.2.1 / 4154-b75d5e62** on AMD Radeon Graphics:

- OpenGL backend.
- Nearest display filtering, matching Linux's crisp ASCII presentation. Use
  `make vita3k VITA3K_SCREEN_FILTER=Bilinear` to explicitly prefer softer edges.
  Compare at a 960x544 viewport or an integer multiple before judging lighting;
  fractional window scaling changes glyph appearance, not the scene lighting.
- Hashless texture cache off; ordinary texture cache on (required for distinct
  scene/material/font samplers in the native GPU renderer).
- Memory mapping disabled.

The template in `cmake/vita3k.yml.in` generates these settings. Do not override
them with Vulkan without rechecking runtime behavior. This changes only the
emulator's host graphics backend; the Vita app uses its dedicated native GXM
scene renderer, independent of SDL's private graphics state.

The failed Vulkan runs attempted to write into read-only cached texture pages
through the emulator's `sceClibMemcpy` HLE, producing repeated host SIGSEGVs.
GDB confirmed an access-permission fault at the texture destination. The
OpenGL/no-hashless-cache configuration avoids that fault in the tested build;
this is not evidence that all Vulkan drivers/configurations fail, or proof that
real Vita runtime behavior matches the emulator.

Vita stdout/stderr files did not flush useful data in this emulator setup, so
startup, periodic progress, profiling, smoke results, and shutdown also use raw
Vita stage-file writes. Those writes occur outside renderer timing sections
except for the one-time startup/presentation markers.

The tested emulator's file-mode translator also ignores `O_TRUNC` on an
existing file. Settings are written to a freshly created staging file and
renamed over the committed file. The corruption regression test creates a
fresh one-byte fixture rather than assuming emulator `fopen("wb")` truncates.
Actual read/write/replacement/rejection assertions still run in the Vita guest.
