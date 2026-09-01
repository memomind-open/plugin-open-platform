# Tetris plugin

Firmware Tetris rewritten as a standalone GMP. It uses Host `on_loop` instead
of `lv_timer`, compact 16-bit tetromino masks, and no libc symbols.

- Turn head left/right: move
- Accessory left/right: move; up: rotate; down: drop one row
- Single/double click: rotate
- Hold: fast drop
- Preview the next tetromino beside the board
- Completed lines flash, then wipe inward before the board collapses
- Keep head raised through the exit countdown: exit
- Click after game over: restart
- Supports all 11 Host languages, with English fallback

After changing this example, run `./gm-build` from the SDK root. The package is
written to `build-host/game/tetris/tetris.gmp` and served through the QR flow.
