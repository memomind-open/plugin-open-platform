# Momo Talking Pet

This device example is the native companion for the WebSDK `talking-pet`
example. It draws only the center playroom and character through the native
GRAY4 framebuffer. LVGL exclusively owns the header and both side panels,
including their borders, labels, dots, dividers, and status bars. The two
rendering systems therefore never write the same pixels. It does not receive
framebuffer images from the phone. Eight
local 144x184 sprite poses keep the character expressive without spending
Bluetooth bandwidth on images.
The source sprite artwork includes oversized optical-black glasses, reinforced
mouth contours, and thicker closed-eye lines. The lenses remain transparent so
star eyes and expressions stay visible, while the dark details remain distinct
on an approximately eight-level monochrome display. The sleeping pose uses a
separately aligned side-view frame. The build script only scales, quantizes,
and compresses these authored image details; it does not draw facial features.
The eight source poses are authored independently from a clean character
reference, preventing layered frames, shifted eyes, patched mouths, or visual
details leaking between props and the face.
The playing pose owns its soccer ball in the sprite itself; the runtime does
not draw a second procedural ball over the character.
The sprite build uses per-pose luminance stretching, mild contrast enhancement,
and edge sharpening. It keeps surfaces continuous while using nearly the full
GRAY4 range, avoiding both flat optical blocks and visible halftone holes. The
top three optical levels are reserved for eyes and specular highlights so they
remain distinct from the light-gray head on a monochrome green display. The
pet keeps a continuous but subdued base fill capped at level 6, while
levels 13-15 remain reserved for the eyes and small highlights. This keeps the
face readable without turning it into sparse line art. The smaller sprite
canvas also reduces the packaged device plugin while retaining all eight poses.
All three HUD panels use optical black fills; their borders, labels, status
bars, and the pet remain visible without a dim green background wash.

Animation frames submit only the center rectangle at `x=182..417,
y=51..349`. Header and side-panel LVGL objects remain outside that dirty region,
preventing framebuffer animation from erasing text or breaking panel borders.
Status changes resize the LVGL bars directly without a framebuffer redraw.

The phone sends a seven-byte state packet on channel `0x4D50` containing the
protocol version, mood, three status values, and level. The glasses runs idle
breathing and blinking plus dedicated happy, eating, playing, sleeping,
listening, and talking animations locally at 12.5 FPS. Each action combines
sprite pose changes, motion, and small particles or sound cues. Button events
use the standard `gm.device-events` uplink channel. Pet actions
are controlled by the phone state packet; the device plugin does not subscribe
to or process IMU gesture events.

The checked-in `momo_sprites.h` is reproducible from the transparent sprite
sheet. Install Pillow only when regenerating this optional asset:

```sh
python3 examples/talking_pet/build_sprites.py
```
