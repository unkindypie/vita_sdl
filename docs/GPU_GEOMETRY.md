# GPU geometry and presentation

The GPU rewrite now has a native GXM backend on Vita and an OpenGL backend on
Linux. They own their contexts, static vertex/index buffers, color/depth targets,
shaders and presentation. No SDL renderer internals are accessed. SDL remains
the event/window/timer boundary (and audio/input boundary for normal gameplay).

This is **not yet the playable GPU ASCII renderer**. Normal gameplay still uses
legacy rasterization while geometry, material/ASCII passes, sprite/UI composition
and gameplay-query decoupling are implemented. The user requests removing the
legacy renderer and obsolete presenter experiments after the migration gates;
they are not permanent fallback backends.

## Ownership and resource limits

- Engine geometry contracts contain no game, SDL or GPU types. Multiple batches
  share one bounded static upload (16 MiB maximum); each draw retains local U16
  indices. Invalid input is rejected before replacing valid geometry.
  Game-independent instance records reference uploaded batches with separate
  model matrices. Repeated instances share GPU vertex/index data; model/view
  transforms run in the vertex shader, not a CPU vertex-transform loop.
- A graphics factory owns native GXM lifetime and immutable compiled GXP data.
  Devices retain that lifetime, own their contexts/patchers/targets and finish
  outstanding work before releasing mapped allocations. Shader compilation is
  once per native graphics runtime, not per frame or target recreation. There
  is no disk cache or automatic proprietary-module download.
- Declared surface samplers always have valid storage, including a neutral
  fallback for direct-color geometry. Do not rely on a dynamic shader branch
  to suppress an unbound texture fetch; compilers can hoist texture operations.
- Recreate targets transactionally: allocate and validate the replacement while
  retaining the active device, then release the previous device. Besides keeping
  the last frame on allocation failure, this avoids the observed Vita3K OpenGL
  address-recycling failure when a larger target immediately replaces a freed
  smaller one. Immediate free/reallocate is not claimed as verified.
- GXM presents through three 960x544 color buffers, sync objects and its own
  display queue, sampling the offscreen texture entirely on GPU. Linux uses
  framebuffer blitting into its own resizable GL window. No framebuffer or
  depth readback occurs in presentation.
- The first offscreen GXM implementation serializes target reuse with
  `sceGxmFinish`. Replace that wait with measured frame-slot/resource fences
  during performance work; these diagnostic timings are not shipping-game FPS.

## Diagnostics

```sh
make test       # includes software-Mesa geometry and real-map rendering
make gpu-test   # desktop accelerated checks and captures under build/linux/
make run ARGS='--gpu-fixture --frames 120 --screenshot build/linux/gpu-fixture.bmp'
make run ARGS='--gpu-fixture --assets assets --frames 120 --screenshot build/linux/gpu-world.bmp'
make vita3k-gpu-test # compiler installed in the selected emulator profile
```

The pinned emulator download currently returns 404; the recorded validation uses
the already cached exact build with `VITA3K=/tmp/asciicker-vita3k/Vita3K` and
`VITA3K_DIR=build/vita3k-shader-probe`. That local override is not a contributor
setup requirement or a fixed download. A separately validated pin update remains
necessary for fresh setup.

Native fixtures require `ur0:data/libshacccg.suprx`. Missing prerequisites fail
visibly; normal gameplay still boots without it. GPU readback tests require
Vita3K surface sync, enabled by `vita3k-gpu-test`, not ordinary performance runs.
The emulator's ordinary texture cache is now enabled while hashless caching
remains disabled. In the pinned source, `bind_texture_without_cache` binds
`cache.textures[0]` for every sampler unit: a three-texture ASCII shader then
samples the last uploaded font as its material table and produces white terrain.
Enabling normal caching supplies independent objects and fixes the captured
image and material/glyph pixel checks. Native Vita has no such cache setting.

The geometry fixture submits near geometry before farther geometry to detect
absent depth testing. It checks actual pixels, asymmetric vertical orientation,
rotation, independent U16 draw ranges, shared-resource instance transforms,
invalid resource rejection, deterministic clear/reuse, display pixels
and 127x93 -> 244x140 -> 127x93 transactional recreation. Readback is diagnostic
only. No CPU rasterizer is used to produce the fixture.

The world fixture copies every patch's height/diagonal/visual/dark data through
the game adapter, builds 102 spatial chunks (maximum 64 patches each), and uploads
121,900 vertices / 156,032 triangles once. Negative chunk coordinates use floor
division. Camera elevation comes from a world-space terrain ray, not render
depth. Geometry uses normalized elevation (`upstream z / 16`). Fifteen mesh
resources are uploaded once for 1,083 instances. Conservative local-bound
culling reduces the recorded view from 1,185 submitted instances to 49 draws
without changing the geometry image. Forty-eight mesh line primitives still
need a dedicated edge pass; they are counted, not silently claimed as drawn.

Terrain now also carries a 1024x512 point-sampled surface atlas with material
IDs, shadow bits and elevation flags, with duplicated one-texel patch gutters.
Vertex shaders light encoded terrain normals; the scene target contains material
metadata. A shared GL/Cg resolve selects material glyphs and their foreground/
background colors, resolves four samples per cell, and samples the real CP437
font. Material colors use the same quantization and pastel grading as the UI.
Direct-color mesh samples use CP437 half-block/density glyphs instead of smooth
polygon output. This is an initial ASCII pass, not final visual parity: mixed
material blending, depth edges, mesh lighting, water, reflections and sprites
remain. No CPU scene pass or readback is involved in drawing or presenting it.

The final capture is the resolved display, not material bytes. An additional
`-ascii.bmp` capture exercises all 256 material IDs with varying light/elevation.
Its GPU output is checked against independently addressed material/font texels
with a one-byte blending tolerance, excluding exact interpolation boundaries.
The additional mixed-material regression places two different upper material
samples above a matching lower pair; the lower pair must win the majority.
Host tests also exhaust all 16,384 material entries and all 16,384 font texels,
atlas gutters and failed-load transactionality. Collection/upload and render/
present wall-clock timings remain separate; display waits are not GPU work time.

ASan checks remain enabled for GPU fixtures. Leak detection alone is disabled
for the two EGL/llvmpipe tests because the Fedora SDL2-compat/SDL3 driver/JIT
retains allocations after teardown. All non-GL tests retain leak detection.
This limitation is recorded explicitly, not treated as leak-free GPU validation.
