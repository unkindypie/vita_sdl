# Running the Vita build in Vita3K

Install/extract the official Linux Vita3K emulator separately. It is a host
debugging tool, not an application dependency. These commands build the VPK,
generate a known-working config, and install/run it with the SDL frontend:

```sh
make vita3k VITA3K=/path/to/Vita3K
make vita3k-smoke VITA3K=/path/to/Vita3K
```

The smoke runs the actual ARM Vita binary for 600 frames with synthetic movement
and camera events. It should append `SMOKE_OK` with all three checks set to 1,
then `SHUTDOWN_OK`, to:

`build/vita3k/storage/ux0/data/asciicker/stages.log`

The framebuffer capture is alongside it as `vita3k.bmp`. This is the application's
CPU framebuffer, not a capture of the emulator window. The stage log also
records successful presentation and progress. Logs append across launches;
inspect the latest boot section. Emulator exit status alone is not proof of a
successful guest smoke. An early exit or `SMOKE_FAIL` needs investigation.

`make vita3k VITA3K_ARGS='--smoke, --seconds, 60'` supports longer checks.
Vita3K guest arguments must be comma-and-space separated. Select exits normal
play. `make clean` also removes this generated emulator storage, including its
installed preview, settings, screenshots, and logs; it does not affect your
normal Vita3K storage.

## Working graphics settings

Validated with Linux Vita3K **0.2.1 / 4154-b75d5e62** on AMD Radeon Graphics:

- OpenGL backend.
- Hashless texture cache off; texture cache off.
- Memory mapping disabled.

The template in `cmake/vita3k.yml.in` generates these settings. Do not override
them with Vulkan without rechecking runtime behavior. This changes only the
emulator's host graphics backend; the Vita app still uses SDL2's native GXM
renderer and the engine still renders the scene on the CPU.

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
