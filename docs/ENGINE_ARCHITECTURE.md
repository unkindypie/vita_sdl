# Engine/game boundaries

One shared C++17 codebase targets Linux/OpenGL and Vita/native GXM. SDL supplies
events, controllers and audio. Normal presentation requires a GPU backend;
there is no software scene renderer or SDL glyph-presenter fallback.

## Targets and ownership

- `engine_render`: cells, frame/camera contracts, geometry packing/bounds,
  transforms, culling and ray math. Its public headers contain no SDL, native
  graphics APIs, Asciicker types or game rules.
- `graphics_device`: dedicated GL/GXM context, shader compilation, GPU buffers,
  depth, material/font/overlay textures, reflection and two-pass ASCII output.
  `DeviceFactory` owns shared compiler/runtime storage across device recreation.
- `asciicker_game`: upstream gameplay, physics, world/assets, QuickJS bridge,
  audio and game adapters under `src/games/asciicker/`. `GpuScene` translates
  this game's resources and immutable per-frame camera/light metadata into
  engine geometry. It does not advance simulation or scripts.
- `ascii_assets`: font decoding, shared palette/pastel grading and diagnostic
  pixel helpers. No SDL/native graphics ownership.
- Application: SDL lifecycle/input, scene ownership, renderer attachment,
  independent overlay upload, presentation and bounded profiling.

`render_bridge.cpp` is the small compatibility boundary for upstream's opaque
`Renderer` API. It stores a borrowed `SceneBackend`, camera readiness/errors and
game-owned proximity pointers, not a sample buffer. Missing backends fail
explicitly. Failed frames invalidate camera state and never trigger a fallback.
Projection/picking outputs commit only after successful finite results.

Detach/destroy `GpuScene` before releasing its world or graphics device. New
Game destroys the adapter, unloads world/scripts/audio, loads fresh services and
resources, then reattaches. Do not retain pointers across this boundary.
GPU readback is allowed only for explicit fixtures/screenshots, never picking,
proximity, UI generation or normal presentation.

## New game modules

A future platformer/roguelike supplies its own simulation, assets and rules,
then produces engine geometry/instances, camera metadata and overlay cells.
It does not need to subclass Asciicker's `Game` or inherit its terrain/world
formats. Keep native APIs behind `graphics_device` and game-specific translation
inside adapters. Reusable rendering is implemented; generic campaign/game-rule
frameworks and a second complete game are not part of this port.

The legacy Asciicker `Game::Render` still orchestrates one simulation tick,
scene dispatch and UI. Extracting that orchestration is separate remaining work;
the renderer interface itself is already simulation-free. Do not claim a rename
alone separates update from presentation.

## Validation

Engine contracts/camera/geometry tests require no graphics context. A bounded
`SimulationView` test double exercises gameplay and collision without drawing;
it is not a shipping software renderer. Actual GL/GXM fixtures check geometry,
depth, materials, ASCII coverage, sprites/UI, dirty-atlas stride and recreation.
Full GPU gameplay smoke checks combat/inventory/menus/scripts/audio/New Game.
Commands and hardware limitations are recorded in [VALIDATION.md](VALIDATION.md).
