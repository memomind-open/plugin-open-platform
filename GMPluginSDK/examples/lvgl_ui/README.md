# LVGL UI

Uses the firmware-owned core LVGL drawing table to create a label and animated arc. The
plugin contains no LVGL engine, draw buffer or timer; animation runs in
`on_loop`.

After changing this example, run `./gm-build` from the SDK root.
