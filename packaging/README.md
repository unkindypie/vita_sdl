# Vita bubble icon

`IMG_4564.JPG` is the user-supplied source photo, copied into this repository
without removing the original. `icon.png` is its square face/upper-body crop.
The old `icon.svg` is retained as an unused previous design.

To reproduce the icon using ImageMagick:

```sh
magick packaging/IMG_4564.JPG -auto-orient -crop 2800x2800+1000+0 +repage \
  -resize 128x128 -strip -alpha off -colors 256 -depth 8 -interlace none \
  PNG8:packaging/icon.png
make verify-vpk
```

The icon must be 128x128, 8-bit **indexed** PNG, non-interlaced. An 8-bit/channel
RGBA PNG is not equivalent and can cause VitaShell installation error
`0x8010113D`. Both CMake configuration and the Linux `packaging_icon` test check
the PNG header. The converted icon contains no source-photo EXIF metadata.
The source JPEG is not included in the installed VPK.
