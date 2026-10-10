# Distribution notices and asset provenance

This is a modified native SDL2 port, not the original Asciicker distribution.

- Asciicker code, maps, meshes, sprites, font, palette and the `forest.ogg` /
  `footsteps.ogg` samples are copied from msokalski/asciicker commit
  `8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635`. The repository supplies an MIT
  license, retained as `asciicker/LICENSE`. No additional author/recording
  attribution was supplied beside those sound files in that snapshot. This
  records provenance, not an independent guarantee of third-party asset rights.
- QuickJS 2026-06-04: Fabrice Bellard and Charlie Gordon; MIT license retained
  in `quickjs/LICENSE`.
- SDL2 2.32.10: Sam Lantinga and contributors; zlib notice in `SDL2-LICENSE.txt`.
- uPNG: Lode Vandevenne and Sean Middleditch, modified by msokalski; zlib notice
  retained in `asciicker/upng.c`.
  tinfl: Rich Geldreich; public-domain notice retained in `asciicker/tinfl.c`.
- stb_vorbis v1.09: Sean Barrett and contributors; public-domain/perpetual grant
  retained at the top of `asciicker/stb_vorbis.cpp`, including upstream cue
  marker support. No external Vorbis runtime library is needed.
- VitaSDK/newlib and its static support libraries are provided by the pinned
  development image. Preserve their applicable notices when redistributing
  toolchains; the application does not bundle a toolchain or firmware.

The editable port icon is an original vector in `packaging/icon.svg`; its PNG
is a checked-in packaging asset, not build output. Regenerate deliberately with
ImageMagick: `magick -background none packaging/icon.svg -depth 8 PNG32:packaging/icon.png`.
ImageMagick is not required for normal compilation or installation.
