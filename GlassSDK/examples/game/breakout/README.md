# GM Breakout plugin

This is a plugin port of the firmware `breakout_game_app.c`. It uses only the
public GM Plugin ABI and the Host-owned LVGL runtime, so it is not linked into
the glasses firmware ROM.

Version 4 uses the final Input extension (ID 4), replacing the unpublished Pointer
draft. Rebuild/update draft firmware and plugins together; unsupported older
firmware still falls back to ordinary button/IMU control.

Controls:

- turn the head left/right: move the paddle;
- mouse relative X/Y: move the paddle horizontally/vertically (one delta unit
  per pixel, clamped to the board);
- mouse wheel: move the paddle vertically, 12 pixels per notch (positive/up
  scroll moves up); horizontal wheel is unbound;
- mouse right-click / accessory BACK: exit immediately, also when paused or over;
- press the accessory left/right buttons: move the paddle one step (holding a
  repeatable button continues moving it);
- single-click: pause/resume, or restart after win/game over;
- raw KEY Left/Right (down/repeat): move horizontally;
- mouse left, Select, gamepad A or TOUCH_TAP (down): pause/resume/restart;
- ABS X/RX: set horizontal position. Centered gamepad norm_value [-1000,1000]
  maps to the full paddle range; other ABS values are treated as pixel positions
  and clamped (device-specific ranges need calibration outside this example);
- touch: 2-D x movement or 1-D digitizer X/Y changes move horizontally, one raw
  coordinate unit per pixel. First sample anchors; UP/CANCEL/suspend resets it.
  Pressure does not move the paddle. Raw contact DOWN/UP is not synthesized into
  a tap; use the Host-recognized TOUCH_TAP gesture;
- keep the head raised for three seconds: exit the plugin application;
- long-press: show a two-second exit countdown; release to cancel.

Input extension ID 4 is optional. All four classes are subscribed at once.
Unsupported firmware, an absent table,
or failed subscription leaves head and button controls working. The Host pauses
input delivery under overlays and revokes it on stop/unload; the plugin retries
subscription on resume and explicitly calls `unsubscribe()` in `on_stop` when
the extension is available. Movement is ignored during pause, game-over and exit.
Restart restores the paddle's initial vertical position. This does not change
the core ABI or introduce any Bridge version check.

See [HOGP Input](../../../docs/INPUT.md) for the API and validation.

Wheel and right-click behavior follows firmware commit `87ef454b`. Raw
MOUSE_RIGHT/BACK uses the same exit path as cooked BACK/TRIGGER, not the RIGHT
direction button. Older firmware can deliver SCROLL_UP/DOWN/TRIGGER steps;
these are used only without an active input subscription to avoid duplicates.

Source review: `python3 build.py build --example game/breakout` from the SDK root
also emits `breakout.review-source.enc` and `breakout.review.json` beside the GMP.
For external headers, see [Source Review Packages and External Headers](../../../docs/REVIEW_PACKAGES.md),
including how to use `--project`, `--source-root`, and `--include-dir` together.
The snapshot contains plugin/SDK source, build/link scripts, and the full original
contents of GCC dependencies, including system headers. `build.json` records
compiler version, commands, file hashes and portable system include directories.
Extraction needs no locally installed headers. Rebuild uses the bundled headers
with `-nostdinc` and verifies their hashes first; a matching compiler is still
required. Studio includes the snapshot in its audit ZIP. Its simulator runs the
compiled GMP, and the phone transfers only GMP data to the glasses.
