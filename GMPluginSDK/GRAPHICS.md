# GM Plugin Graphics

The firmware owns the only LVGL runtime and the physical framebuffer. Plugins
can use shared LVGL for ordinary UI or direct framebuffer access for custom
pixel rendering.

## Shared LVGL

ABI 1.0 exposes the versioned `gm_plugin_lvgl_api_t` table through
`host->graphics.lvgl`. It provides firmware-owned object handles and common
object, style, label, line, and arc operations.

```sh
./gm-build build --example lvgl_ui
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

See [`examples/framebuffer`](examples/framebuffer) for a complete example.

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
