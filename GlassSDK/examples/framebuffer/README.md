# Framebuffer lock/unlock

Demonstrates zero-copy drawing and temporal double buffering with the Host's
existing GRAY_4 framebuffer. It asks the Host for each synchronization slice,
draws one checker stripe directly into that surface, and allocates no pixel
buffer.

The Host completes its empty LVGL root refresh before invoking `on_start`, so
the demo can draw once during `on_start` without a pending Host refresh
immediately overwriting the result.

The important sequence is:

```c
next_y = 0;
while (next_y < display.height) {
    framebuffer.lock(next_y, &surface);
    /* Draw only inside surface.y .. surface.y + surface.height. */
    framebuffer.unlock(&dirty,
        surface.y + surface.height == display.height);
    next_y = surface.y + surface.height;
}
```

Earlier slices use `present=false`, allowing SPI to consume one slice while the
plugin draws the next. Only the final slice uses `present=true`, appending one
display SYNC so the complete update becomes visible together. Never hard-code
the number, height, or boundary of slices; always use the returned `surface`.

This is a low-level alternative to LVGL. Never call an LVGL table function
between `lock` and `unlock`; LVGL already performs its own display
synchronization. Release the surface first, and avoid drawing LVGL objects over
the same pixels because a later LVGL refresh may replace direct framebuffer
contents.
