# GM Plugin Graphics

The firmware owns the only LVGL runtime and the physical framebuffer. Plugins
can use shared LVGL for ordinary UI or direct framebuffer access for custom
pixel rendering.

## Shared LVGL

ABI 1.0 exposes the versioned `gm_plugin_lvgl_api_t` table through
`host->graphics.lvgl`. It provides firmware-owned object handles and common
object, style, label, line, and arc operations.

```sh
python3 build.py build --example lvgl_ui
```

Plugins do not include firmware `lvgl.h`, depend on LVGL private structures,
create a second LVGL heap, allocate another draw buffer, or run another LVGL
timer loop. UI objects are children of the Host-owned plugin root. The Host
deletes that subtree when the visible plugin cycle stops.

Use LVGL for text, controls, and ordinary application UI. Text passed to
`label_set_text` must be NUL-terminated UTF-8. Fonts, text measurement, line
height, next-line offsets, and label selection use Host-owned font resources.
Use `gm_plugin_lvgl_style_number()` and `gm_plugin_lvgl_style_color()` to create
typed style values.

Native LVGL callbacks and timers are intentionally absent because they could
retain plugin function pointers after unload. Use plugin `on_event` and
`on_loop` callbacks instead.

### Indexed images and frame animation

LVGL API 1.1 appends static indexed-image and Host-driven frame-animation
operations. A plugin that uses them must validate both the version and the
expanded table size before reading the appended function pointers:

```c
const gm_plugin_lvgl_api_t *ui = host->graphics.lvgl;
if (!GM_PLUGIN_VERSION_COMPATIBLE(ui->api_version,
                                  GM_PLUGIN_VERSION(1U, 1U)) ||
    ui->struct_size < GM_PLUGIN_LVGL_API_1_1_SIZE)
    return GM_PLUGIN_EVERSION;
```

`GM_PLUGIN_LVGL_IMAGE_INDEXED_4BIT` payloads use this fixed layout:

```text
64-byte palette: 16 consecutive BGRA8888 entries
pixel indexes:    ceil(width / 2) bytes per row, with no row padding
                 even x = high nibble, odd x = low nibble
```

The exact payload size is therefore
`64 + ceil(width / 2) * height` bytes. Palette alpha is composited by LVGL;
the physical display is still monochrome GRAY_4. A fully transparent palette
entry reveals the parent or lower sibling rather than introducing alpha into
the final framebuffer.

The Host copies `gm_plugin_lvgl_image_dsc_t` values and animation frame-pointer
lists during each API call, but pixel payloads are borrowed. Keep their bytes
readable and unchanged until `image_set_source()` replaces the source,
`anim_image_set_sources()` replaces the frame set, or the object is deleted.
Static storage is the simplest safe choice. Prebuilt, immutable pixel arrays
should be `static const` so their bytes can remain in Flash. Mutable static
arrays, such as the writable example below, consume RAM. Relocatable frame
pointer tables can still require RAM even when the pixels are in Flash.

Each payload address must be **4-byte aligned**, because LVGL reads its palette
as 32-bit colors. The Host rejects unaligned payloads. Align both the array base
and each frame's storage stride; do not add padding within image rows or count
trailing storage padding in `data_size`. For example:

```c
#define IMAGE_BYTES (64U + ((WIDTH + 1U) / 2U) * HEIGHT)
#define FRAME_STRIDE ((IMAGE_BYTES + 3U) & ~3U)
_Alignas(4) static uint8_t frames[FRAME_COUNT][FRAME_STRIDE];
/* descriptor.data = frames[index]; descriptor.data_size = IMAGE_BYTES; */
```

The SDK compiler supports `_Alignas(4)`. A 75 by 65 image has 2534 payload
bytes but uses a 2536-byte stride in such an array. Dynamically allocated image
storage must also keep every frame address aligned; adding an arbitrary byte
offset to an aligned allocation can violate the requirement.

`anim_image_create()` creates a stopped animation. Set its frame duration and
repeat count as needed, then call `anim_image_start()`. A repeat count of zero
plays once; `GM_PLUGIN_LVGL_ANIM_REPEAT_INFINITE` loops indefinitely. The Host
pauses running animations while the plugin is suspended and resumes them when
the visible cycle resumes. Position and size continue to use the existing
generic object functions.

Use frame animation sparingly. Plugin-resident pixel storage grows by
`frame_count * (64 + ceil(width / 2) * height)` bytes, before application
state, stack, message buffers, and temporary work memory are considered. Keep
frames small, limit their count, and leave a deliberate runtime memory margin.
A longer frame duration reduces update frequency but does not reduce the bytes
held by the frame set.

See [`examples/image_animation`](../examples/image_animation) for static source
replacement and an infinite Host-driven animation.

## Direct framebuffer

`graphics.framebuffer.lock/unlock` provides zero-copy access to slices of the
firmware GRAY_4 framebuffer. Always use the geometry and stride returned by the
Host; do not hard-code the display size or half-screen boundary.

`unlock(dirty, present)` submits only the dirty rectangle. For a full frame,
draw each returned slice and pass `present=false` for every earlier slice, then
`present=true` for the final slice. A one-slice update passes `true`.

GRAY_4 stores two pixels per byte. An even x coordinate uses the high nibble:

```c
gm_plugin_framebuffer_surface_t surface;
gm_plugin_rect_t point = { .x = 12, .y = 34, .width = 1, .height = 1 };
if (host->graphics.framebuffer.lock(34, &surface) == GM_PLUGIN_OK) {
    uint8_t *pixel = surface.pixels +
        (34U - surface.y) * surface.stride + (12U >> 1);
    *pixel = (uint8_t)((*pixel & 0x0fU) | 0xf0U);
    host->graphics.framebuffer.unlock(&point, true);
}
```

See [`examples/framebuffer`](../examples/framebuffer) for a complete example.

## Do not nest rendering paths

The firmware LVGL port already synchronizes rendering with display scanout.
Never call LVGL while a direct framebuffer surface is locked, even when the
objects appear to occupy different regions. This sequence is invalid and may
stall the display task:

```c
host->graphics.framebuffer.lock(y, &surface);
host->graphics.lvgl->label_set_text(label, "invalid nesting");
host->graphics.framebuffer.unlock(&dirty, true);
```

Prefer one rendering path for each screen or frame. If both paths are used
sequentially, release the framebuffer before calling LVGL and keep their display
regions separate. LVGL retains ownership of its object pixels and a later
invalidation may overwrite direct writes in an overlapping region.
