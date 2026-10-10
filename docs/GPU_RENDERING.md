# GPU migration — experiment and remaining scene work

Authorized 2026-10-10. This is **not an all-GPU engine**. Gameplay, physics,
JavaScript and the original scene rasterizer remain on the CPU. GPU atlas
presentation is implemented and optional; a complete GPU scene replacement is
not implemented. Do not equate the two or promise 60 FPS from this experiment.

## Working experiment

```sh
make run ARGS='--gpu-presenter'
make vita3k VITA3K_ARGS='--gpu-presenter'
make gpu-test # actual accelerated host framebuffer vs CPU reference
```

`--cpu-presenter` is the default/reference. Explicit GPU mode requires an
accelerated SDL renderer and a binary CP437 atlas; errors fail visibly rather
than silently claiming software fallback is GPU rendering.

`GpuPresenter` uploads one 128x128 font atlas at initialization. Each frame
updates preallocated glyph UV/color vertices, then submits two batched passes:
opaque cell backgrounds and alpha-covered glyphs. SDL uses OpenGL on the tested
Linux host and native GXM on Vita. There are no per-cell draw calls, full-screen
pixel uploads, per-frame font uploads, or readbacks in normal GPU play. Colors,
font orientation, bottom-left cell coordinates and nearest sampling are retained.
The unused CPU streaming texture/pixel buffer is not allocated in GPU mode.

The maximum 120x68 grid is intentional: each GXM pass expands to 48,960 vertices,
below its U16 index table. Both passes occupy approximately 1.57 MB of SDL GXM's
2 MiB vertex pool. Separate untextured/textured passes prevent command merging
past the index limit. Bigger grids are rejected rather than overrunning that
backend. No runtime shader compiler, VitaGL, or new firmware dependency is added.

Tests render the actual geometry pipeline and read back all pixels, comparing
exactly with the CPU converter: every glyph/palette index, asymmetric synthetic
and bundled fonts, repeated changed frames, full-size grid and bounds. Headless
CTest uses SDL's software implementation; `make gpu-test` separately requires an
accelerated backend and does real GPU readback. Vita3K gameplay smoke exercises
the native GXM geometry path. Neither proves real-device performance.

`--screenshot` with GPU mode intentionally saves the **CPU reference** of the
final cell buffer, labelled `SCREENSHOT_CPU_REFERENCE_OK`, not GPU readback.
Capture the emulator window for visual GPU inspection. Readback parity is the
separate host test above. The `glyph_p50` stage includes CPU vertex preparation;
`upload_p50` includes geometry submission; GPU work/waits may occur later.

Initial same-route 600-frame Vita3K measurement: CPU 24.54 ms median, GPU atlas
24.76 ms. **No useful improvement was established**, so CPU stays the default.
Submission offsets the avoided glyph expansion; approximately 22 ms of CPU
scene work remains. See `VALIDATION.md` for later results and verification.

## Full scene migration (not yet implemented)

1. **Split update from rendering.** `Game::Render` currently combines simulation,
   scripts and drawing. Extract one immutable frame snapshot and preserve update
   order/timestamps. Both scene backends must consume the same snapshot without
   updating physics or executing scripts twice. First acceptance: existing CPU
   fixed-route frames and gameplay checks remain unchanged after this refactor.
2. **Establish native GPU ownership and shader builds.** Keep SDL input/audio,
   but create a dedicated Linux GL / Vita GXM rendering boundary with explicit
   lifecycle, render targets, depth and offline shader compilation. SDL geometry
   has no scene-depth/material API; do not blindly share its private GXM context.
   The current project image contains no custom GXP shader compiler. Select a
   reproducible, legally distributable compiler/artifact workflow before changing
   toolchains; do not introduce an undocumented runtime shader requirement.
3. **Terrain and meshes.** Upload static geometry/material data once, retain
   culling, and render the original 244x140 sample grid (2x cells plus border)
   with depth and material/lighting metadata. Begin with a fixed camera, then
   perspective, yaw, clipping, elevation and all map patches. Keep sample data
   resident on the GPU; continuous CPU readback would undermine the migration.
4. **GPU ASCII resolution.** Reproduce the four-sample material/glyph chooser,
   palette quantization, elevation edges and neighbor-dependent rules in shader
   passes, then resolve cells through the font atlas. This is not a grayscale
   ASCII filter over a conventional 3D render. Prove negative-coordinate rounding,
   material seams, coverage/depth and glyph/color choices with small fixtures.
5. **Water, sprites and overlays.** Add reflected geometry, shoreline mixing,
   animated sprites, player/NPC/item occlusion and interaction outlines. Combat,
   inventory, menus and controller text/UI must retain ordering and readability.
   Compare fixed-camera captures at multiple locations, water boundaries and
   perspectives; never hide missing geometry by relaxing pixel tolerances.
6. **Adoption gate.** Run matched CPU/GPU routes with JS/audio, without parity
   injection. Report end-to-end median/p95, submission/wait stages, memory and
   GPU/backend limits. Require a repeatable improvement beyond run noise and
   passing gameplay/visual checks before switching defaults. Physical Vita
   performance and memory acceptance remain separate from emulator results.

The expensive scene work is the target of steps 3–5. Moving physics, scripting,
menu state or inventory ownership to the GPU is neither required nor desirable.
