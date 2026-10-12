# GPU performance and art recovery — 2026-10-12

## Target and interpretation

The target is sustained 60 FPS on physical Vita, with JavaScript, audio and
complete offline gameplay. Hardware acceptance remains **unverified/failed at
the reported 3–4 FPS baseline**. Emulator throughput cannot certify hardware
throughput or establish a linear emulator/device speed ratio.

Keep three distinct measurements:

- Interactive/manual Qt: ordinary simulation timestamps, native guest VBlank.
- Paced scripted route: controlled timestamps/input, guest VBlank, normal JS/audio.
- Unpaced finite diagnostic: same route, no guest VBlank. This measures emulator
  headroom; the in-game synthetic-time FPS counter is not the wall-clock result.

`PROFILE` uses wall-clock durations. `GPU_PROFILE` includes CPU submission and
synchronization, not hardware GPU timestamp queries. Its sub-stages overlap
`scene_p50`; JavaScript overlaps it too. Never add these to the scene duration.
`game_ui_p50/p95` is the per-frame remainder outside the backend (physics/AI,
scripts, interaction gathering and upstream UI); it is computed before taking
percentiles, not by subtracting unrelated median values. It includes JavaScript.
Current scripted routes move/rotate initially, then settle: they do not prove
sustained combat or worst-case traversal performance.

## Confirmed fixes

1. Host VSync defaults OFF for Qt as well as SDL. Native guest VBlank remains
   enabled during normal play. The old Qt host setting inserted multiple host
   refresh waits into offscreen rendering and made the game look GPU-bound.
2. Native reflection capture rotates allocations/textures instead of making a
   transfer-engine copy followed by CPU waits. Ownership/mapping remains paired
   with each allocation through teardown; no framebuffer CPU readback is added.
   Static geometry, GPU-sampled textures and depth live in CDRAM; CPU-written
   transient geometry/rings stay in uncached main memory. Small CDRAM allocations
   have 256 KiB alignment; total memory/throughput still needs hardware profiling.
3. Terrain depth explicitly stores before the sprite pass reloads it. Native
   tests exercise visible and occluded dynamic geometry at three target sizes.
4. Mesh face normals are cached and directionally lit, with scale/reflection-
   correct inverse-transpose transforms and the upstream lifted diffuse curve.
   Mesh clipping still respects water; only the water plane bypasses clipping.
5. Mesh ASCII resolve chooses vertical/horizontal CP437 half blocks based on
   the four samples, preserving detail previously collapsed horizontally. Mesh
   colours use the shared six-level pastel palette, not approximate RGB grading.
6. `make vita3k-bench` is finite, uncapped and non-intrusive: normal JS/audio,
   no parity harness, no surface-sync readback. Its runner requires fresh
   SMOKE_OK/SHUTDOWN_OK, then closes only the emulator process it launched.

The attempted native sprite-underlay GPU-blit replacement failed rear-depth
regression and was removed. The synchronized underlay transfer stays until a
passing replacement exists. Do not trade missing geometry for benchmark speed.

## Evidence

Earlier Qt same-route 600-frame measurements: host VSync ON median 35.74 ms,
p95 42.48 ms; OFF median 16.66 ms, p95 16.72 ms (guest-paced). The reported
manual session had median approximately 49 ms. These runs motivated the frontend
fix; subsequent lighting/resolve changes must be profiled separately.

An initial recovery 3600-frame Qt unpaced route measured median 5.30 ms,
p95 7.97 ms, elapsed 20.6 s (~175 frames/s overall). JS median/p95 was
0.014/0.030 ms; errors and dropped audio commands were zero. The matching no-JS
run measured median 5.68 ms, p95 8.52 ms, with host-test contention: the difference
is noise, not evidence that JS makes rendering faster. Final measurements are
recorded in `VALIDATION.md` with explicit commands and limitations.

Final controlled runs after native target rotation/art repairs: Qt with JS/audio
median 5.27 ms/p95 7.93 ms (~176 frames/s overall), no-JS 5.14/7.75 ms; SDL JS
5.17/7.80 ms (~180 frames/s overall). Final paced Qt runs measured 16.66 ms
median with host VSync both ON and OFF (p95 16.71/16.72 ms). Thus the initial
slowdown cannot be attributed solely to a frontend toggle: native resource/
buffer changes and host presentation state matter too. OFF remains the project
default to avoid redundant host pacing; normal guest VBlank is unchanged.
An ordinary 30-second Qt run, using real simulation timestamps and no synthetic
input, rendered 1798 frames (~59.9 FPS overall), median/p95 16.66/16.71 ms,
JS 0.019/0.047 ms, with zero script errors or dropped audio. This independently
checks idle gameplay timing rather than only the scripted timestamp path.

Headless and native fixtures cover lit/shadow/perpendicular light, authored unit
normals, inverse-transpose scale/reflection, front/rear sprite depth, target reuse,
materials, mixed domains, transparent sprite channels, independent UI and partial
terrain updates. The new mesh-half-block fixture checks 458,752 nonboundary native
display pixels against CP437 coverage and the shared palette.

## Remaining gates

Physical Vita stage logs are needed to locate its actual limit. Preserve clock/
device details, measure movement, combat and water, and require median/p95 plus
sustained play before claiming 60 FPS. Do not lower resolution, disable scripts,
remove reflections or change device clocks silently. Transparent overlapping
sprite layers and water/reflection visual parity remain separate audit items in
`GPU_REWRITE_PLAN.md`; these art repairs are not a claim of exact CPU pixel parity.
