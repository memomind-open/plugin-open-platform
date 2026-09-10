# GM Breakout plugin

This is a plugin port of the firmware `breakout_game_app.c`. It uses only the
public GM Plugin ABI and the Host-owned LVGL runtime, so it is not linked into
the glasses firmware ROM.

Controls:

- turn the head left/right: move the paddle;
- press the accessory left/right buttons: move the paddle one step (holding a
  repeatable button continues moving it);
- single-click: pause/resume, or restart after win/game over;
- keep the head raised for three seconds: exit the plugin application;
- long-press: show a two-second exit countdown; release to cancel.

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
