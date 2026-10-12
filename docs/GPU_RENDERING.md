# GPU rendering

Normal `make run` and Vita launches use the dedicated OpenGL/native GXM scene
renderer. The CPU scene rasterizer and obsolete SDL glyph-only presenter are
removed. `--gpu-scene` remains an accepted compatibility alias, not a switch.
There is no CPU scene fallback or normal scene/depth readback.

## Pipeline

1. Cache terrain chunks, shared mesh resources/instances and a guttered material
   atlas at scene load. Dirty blood materials update only affected atlas regions.
2. Collect animated sprite-cell quads, projectiles and health indicators. Camera
   picking uses collision rays, never GPU readback. Gameplay owns proximity lists
   and projectile damage independently of rendering.
3. Render reflected geometry/sprites, then primary geometry, water and sprites
   with GPU depth. Authored mesh line edges become narrow depth-tested ribbons.
4. Resolve material/lighting/elevation metadata into two RGBA style texels per
   ASCII cell. Material IDs and direct mesh RGB must not be averaged together.
5. Expand CP437 coverage, compose sprites and independent CPU-generated UI,
   then present. Terrain/material/font textures remain point-sampled.

Linux presentation letterboxes/pillarboxes to the native Vita aspect ratio,
including matching mouse/touch coordinates. Diagnostic display readback returns
the game viewport without host black bars. Native Vita remains 960x544.

Shader sources are separate `.glsl` and `.cg` files under `shaders/`. CMake
embeds them and shared includes; no shader directory is needed at runtime.
Native GXM requires the separately installed `ur0:data/libshacccg.suprx` compiler.
Missing compiler/shader/pass failures are explicit errors. The VPK does not
redistribute the module. SDL owns input/audio; GXM owns its own graphics context.

## Checks and interpretation

`make test` exercises headless Mesa geometry/material/composition fixtures,
gameplay simulation with a non-rasterizing view double, and full playable GPU
parity. `make gpu-test` uses the actual desktop GPU. `make vita3k-gpu-test`
checks native geometry/ASCII pixels; `make vita3k-smoke` exercises gameplay/reset.
GPU screenshots are labelled `SCREENSHOT_GPU_OK`; readback is diagnostic only.

`GPU_PROFILE` reports CPU wall time, including synchronization, for collection,
reflection, primary submission, UI preparation and present waits. These overlap
the enclosing scene timer and are not GPU timestamp queries. Use matched normal
JS/no-JS routes without `--verify-gameplay` for benchmarks; that harness changes
gameplay, reloads the world and intentionally injects script failures.

Remaining gates include overlapping transparent sprite composition/visual audit,
legacy game update/presentation separation and real-device performance/input
validation. Water reflection styling and mesh line thickness intentionally
differ from the old software implementation. Do not claim exact visual parity
or physical Vita FPS from emulator results. See [the active plan](../GPU_REWRITE_PLAN.md)
and [validation evidence](VALIDATION.md).
