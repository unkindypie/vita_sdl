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
controls and the intentionally deferred audio/script state.

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
Scene time includes simulation and software rendering. Upload includes SDL copy
submission; presentation may include GPU/vsync waiting. These are diagnostic
CPU timings, not GPU timestamps or real Vita performance measurements.

Streaming-texture upload uses SDL lock/unlock and copies rows using the returned
pitch. Startup stages are recorded separately; on Vita a raw append-only stage
file survives failures that prevent stdio from flushing.

GPU glyph presentation and GPU scene rendering are explicitly deferred. Keep
the deterministic CPU converter as a reference if either is explored later.

## CPU presenter optimization

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
