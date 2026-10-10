# ASCII rendering and diagnostics

The shared engine renders the original map into a 120x68 `AnsiCell` buffer on
the CPU. Each cell is rasterized using the upstream CP437 8x8 font and its
6x6x6 RGB palette into a 960x544 ARGB8888 pixel buffer. SDL2 uploads one streaming
texture and presents it. Engine cells use a bottom-left origin; the presenter
flips cell rows while preserving font-atlas orientation.

Linux uses SDL's available accelerated renderer, with software fallback for
headless tests. Vita uses upstream SDL2's native GXM backend. No desktop GL
context, VitaGL shader compiler, editor, or GPU scene renderer is required.
Desktop resizing uses SDL logical-size letterboxing. Two footer rows display
controls and the current audio/script state.
The presentation palette now applies a gentle pastel grade: 25% luminance
desaturation, lifted slate shadows (RGB 44/48/56), and bounded highlights
(240/244/252). Black terrain marks and particles become less harsh without
removing their glyphs or changing engine lighting, visibility, or materials.
Both CPU and GPU presenters share a constexpr lookup; there is no extra
per-pixel grading pass. The original engine palette remains available as the
reference `palette_color`; `display_color` provides the graded colours.
This grade also affects HUD/text, keeping a consistent look.

CPU texture scaling uses linear sampling; Vita3K uses `Bilinear` for a softer
final display. Restart `make vita3k` to regenerate its configuration. Native
960x544 rendering retains the original glyph coverage; filtering smooths scaled
edges rather than reducing scene resolution. The optional GPU font atlas stays
nearest-sampled to avoid bleeding between glyphs; emulator display smoothing
still applies to either presenter.

`make run ARGS='--smoke --frames 240 --screenshot build/linux/smoke.bmp'` exercises
the actual SDL event path, upstream movement/physics, camera rotation, glyph
conversion, texture upload, and shutdown. Synthetic time advances 16,667 us per
frame for this test; the upstream displayed FPS is therefore synthetic, not a
benchmark. `PROFILE` records wall-clock stage times separately.

`--frames N` exits after N rendered frames. `--screenshot FILE.bmp` saves the
presenter's final framebuffer. `--assets DIR` and `--manifest FILE` override
locations; all manifest files are checked before unsafe upstream sprite loading.
The bundled assets are trusted engine data, not a hardened user-file import API.

Profiling stores at most 3,600 frame samples for each stage and reports median,
p95, and maximum total time plus median scene/glyph/upload/presentation times.
For longer runs, the summary describes the latest 3,600 frames, not all-time
percentiles or an all-time maximum. Memory is sampled once per minute on Linux.
Scene time includes simulation, JavaScript and software rendering. `js_p50` /
`js_p95` measure script callback work inside that scene interval, not an extra
stage to add to it. `SCRIPT_PROFILE` reports cumulative calls, exceptions and
runtime-accounted heap usage; intentional parity-harness failures are expected.
Use runs without `--verify-gameplay` for performance comparisons. Upload includes SDL copy
submission; presentation may include GPU/vsync waiting. These are diagnostic
CPU timings, not GPU timestamps or real Vita performance measurements.

Streaming-texture upload uses SDL lock/unlock and copies rows using the returned
pitch. Startup stages are recorded separately; on Vita a raw append-only stage
file survives failures that prevent stdio from flushing.

Optional GPU glyph presentation now uses a batched font atlas via SDL geometry
(`--gpu-presenter`). It is not a GPU scene renderer and is not the default:
initial measurements did not demonstrate an end-to-end Vita3K gain. The CPU
converter remains the deterministic reference. See [GPU migration](GPU_RENDERING.md)
for commands, tests, backend limits and the unfinished full-scene work.

## CPU presenter optimization

Current follow-up: the palette is a constexpr 256-entry lookup rather than
recomputed RGB cube arithmetic per cell. Binary CP437 atlases use branchless
eight-pixel scanlines with memcpy-safe unaligned/padded output. Atlas coverage
is checked per call (no stale address-based cache); nonbinary coverage retains
the exact original rounded blend. Tests compare all palette indices, all glyphs,
mixed binary coverage, orientation and padding, plus all 256 alpha values.
CPU engine/presenter translation units now use `-O3`; engine math also uses
`-fno-math-errno` to permit hardware square root without errno bookkeeping.
No fast-math, reduced cell resolution, skipped simulation, or lowered geometry
quality is enabled. See `VALIDATION.md` for measured results and limitations.
The scene rasterizer's integer-result floor calls use an inline truncation plus
negative-fraction correction. It preserves float/double input precision and
floor results for finite values in the existing int-range coordinate/color
domain, including negative coordinates. Tests cover fractional values and int
boundaries; final fixed-route framebuffers match the pre-optimization bytes on
both Linux and Vita3K. This is not a checked conversion for untrusted numbers.

The following measurements describe the earlier optimization pass:

The presenter is compiled with `-O2` even in the Debug build, retaining debug
symbols, as the upstream scene renderer already was. Previously the Debug
pixel loop was unoptimized, so its timings overstated the production cost.
Binary glyph coverage (`0`/`255`) now selects background/foreground directly;
intermediate coverage retains the original rounded RGB blend. Tests check all
256 coverage values with unaligned output in addition to orientation/pitch.

On the recorded Linux desktop smoke this reduced median glyph time from about
8.93 ms to 0.53 ms, with total median 6.28 ms and p95 8.35 ms. Compiler settings
and the binary-coverage fast path were changed together, so these numbers do
not isolate their individual contributions. No GPU renderer or game-script
changes were made. Scene work is now the largest measured CPU stage; profile
representative scenes before making architectural rendering changes.
