# Linux and Vita visual parity

Both backends use 120x68 ASCII cells, 244x140 geometry samples (including
borders), the same material tables, CP437 font and pastel palette. Vita outputs
960x544. Linux resolves to its letterboxed window viewport, which a tiling
desktop can shrink: comparing differently sized windows is not a shader test.

## Findings, 2026-10-12

Matched deterministic smoke captures, without the gameplay parity harness,
were taken at 960x544 on Linux OpenGL and native GXM in Vita3K:

| Route endpoint | Normalized RGB RMSE |
| --- | --- |
| Frame 240 | 0.00115825 |
| Frame 600 | 0.00115589 |

The captures were inspected alongside difference images. They show matching
terrain contrast, pastel colours, water, entities and HUD placement. Small
differences are concentrated in water/reflections and raster/sample boundaries;
these two scenes do not show a broad gamma or lighting mismatch. This does not
prove every camera, material, transparent overlap or physical-Vita display is
identical. No speculative Vita-only lighting/gamma correction was introduced.

Vita3K previously applied **Bilinear** filtering to the final framebuffer,
unlike Linux's crisp nearest-sampled presentation. The generated config now
defaults to **Nearest**. It affects the emulator window only, not Vita shaders
or native hardware. Compare at a 960x544 viewport or integer multiple; nearest
filtering at fractional scales can still give uneven glyph widths.

For the previous softer display preference:

```sh
make vita3k VITA3K_SCREEN_FILTER=Bilinear
```

Material IDs, font and scene metadata remain point-sampled in both renderers.
Do not blur these textures to soften the final image.

## Repeatable captures

```sh
make visual-captures
```

This uses SDL's offscreen OpenGL driver for Linux so desktop window management
cannot shrink the reference, and the SDL Vita3K frontend with surface sync for
guest readback. The existing separately installed shader compiler is required.
Both runs use the same 600-frame scripted route, normal JS/audio, no injected
gameplay tests. Generated files:

- `build/linux/visual-600.bmp`
- `build/vita3k/storage/ux0/data/asciicker/visual-600.bmp`

Optional ImageMagick, installed on the host, produces a numerical comparison
and a highlighted difference image:

```sh
magick compare -metric RMSE build/linux/visual-600.bmp \
  build/vita3k/storage/ux0/data/asciicker/visual-600.bmp \
  build/linux/visual-600-diff.png
```

ImageMagick returns 1 for non-identical images, even when the difference is
small; 2 indicates an error. The parenthesized number is normalized RMSE.
Always inspect images as well: global error alone can hide a missing entity.
Surface-sync captures and offscreen execution are not hardware/performance
benchmarks. Emulator guest captures also exclude its final host-window filter.
