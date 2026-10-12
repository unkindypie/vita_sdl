# Vita bubble icon

`IMG_6612.PNG` is the current user-supplied source image, copied into this
repository without removing the original. `icon.png` is its square 128x128
conversion, with transparency flattened onto black. The previous generated
icon is retained as `icon-previous.png`, together with its source `IMG_4564.JPG`
and the earlier `icon.svg` design. Only `icon.png` is packaged in the VPK.

To reproduce the icon using ImageMagick:

```sh
magick packaging/IMG_6612.PNG -auto-orient -background black -alpha remove \
  -alpha off -resize 128x128^ -gravity center -extent 128x128 -strip \
  -colors 256 -depth 8 -interlace none \
  PNG8:packaging/icon.png
make verify-vpk
```

The icon must be 128x128, 8-bit **indexed** PNG, non-interlaced. An 8-bit/channel
RGBA PNG is not equivalent and can cause VitaShell installation error
`0x8010113D`. Both CMake configuration and the Linux `packaging_icon` test check
the PNG header. The converted icon contains no source-photo EXIF metadata.
Neither original source image nor previous designs are included in the VPK.
