# Image and animation

This example demonstrates the LVGL 1.1 image API with two compact objects built
from the original ZEN fighter art in `game/fighter_arena`:

- a 75 x 65 static fighter image on the left; and
- a 75 x 65 Host-driven four-pose fighter animation on the right.

Each fighter is created after a fixed panel containing labels and status bars.
The later-created fighter stays above those components: opaque sprite pixels
cover the panel content, while transparent palette index 0 reveals it. This
makes image transparency and LVGL sibling stacking order visible in one demo.

Build it from the GlassSDK directory:

```sh
python3 build.py build --example image_animation
```

On Windows PowerShell, use `py` instead of `python3`.

The sample decodes the fighter's row-compressed 112 x 98 source frames directly
into one-third-size nearest-neighbor indexed-4 images in static storage. It
does not allocate an intermediate full-size bitmap. Production assets normally
come from an offline converter. Each payload contains a 64-byte table of 16
BGRA8888 palette entries followed by packed 4-bit pixel indexes. Even x uses
the high nibble and odd x uses the low nibble. Palette alpha is composited
before the glasses framebuffer is reduced to GRAY_4.

Do not use large animations by default. Indexed frame storage grows with both
pixel area and frame count. A 75 x 65 frame occupies 2,534 bytes, so the four
demo frames reserve 10,136 bytes instead of the 88,064 bytes required by four
224 x 196 frames. Keep enough plugin memory available for application state,
the stack, messages, and temporary work buffers. Reduce dimensions and frame
count before removing those safety margins.

The Host copies descriptors and animation frame lists, but it does not copy
pixel payloads. Keep every payload readable and unchanged until its image source
is replaced or the object is deleted.
