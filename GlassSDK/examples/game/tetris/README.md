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

Version 6 restores the firmware's 60x60 preview at (340, 8), 388-pixel
instruction area and original localized text at the 600x350 reference size.
Raising the head hides the preview until the countdown is cancelled. The
firmware displays 3, 2, 1 starting after the first second, then exits on the
next second; the original text says "3 seconds", but elapsed time is about
four seconds. See [firmware parity](../FIRMWARE_PARITY.md).

Version 7 restores all 28 native rotation masks (including I/S/Z at 180 degrees),
the strict IMU movement debounce and one simulation step per timer callback.
The countdown width includes its borders and uses actual time. The preview
paints above the instructions, and scores no longer wrap at 65,536.

Version 8 commits head-exit once the countdown completes. Simulation, rendering
and game input stop until Host stop, even if callbacks continue or the head is
lowered. Suspend/resume cannot cancel a committed exit; a new start resets it.
The countdown cadence and cancellation before completion remain unchanged.

Version 9 retains the fractional remainder of the 100ms simulation clock across
Host callbacks. Regular 16/33/60ms callbacks and jitter no longer slow gravity,
fast drop or line clearing. A stalled callback still advances only one step,
discarding missed whole steps without discarding the fractional remainder.
