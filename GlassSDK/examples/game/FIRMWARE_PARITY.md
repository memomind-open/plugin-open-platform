# Native game reference and GMP ports

Reference: `glassproject` commit
`09bd6ae5be17a48add0aaf429e8ac43f1dae271b`, the parent of
`9c4d3f3ad94969c13f7f5681d198f58a970a60d2`, which removed the native games.
The reference implementation lives under
`Platform/WQ7036/wq-adk/components/apps/acore/displayapp/src/application/`:
`breakout_game_app.c`, `jet_runner_game_app.c`, and `tetris_game_app.c`.
The shared exit reference is `src/widgets/circle_exit_pb_widget.c`.

These are glasses-only plugins. Clear the phone component in Desktop Studio
when previewing or exporting them independently.

## Restored behavior

| Plugin | Restored details at 600x350 |
| --- | --- |
| Jet Runner v8 | Header labels centered vertically; centered footer; original 11-language text; content-height 260-pixel result panel; antialiased circular hold-exit overlay using the required LVGL API 1.1 images, retained until Host stop; committed BACK exit; 20-pixel accessory target movement; fractional collision bounds; restart-time pitch calibration; accumulated frame duration capped at 100ms |
| Breakout v12 | 28-pixel bricks centered at x=66; bottom-row brick slots; vertical background grid; native paddle/trail styles and one-frame trail lag; content-sized score label, 130-pixel score reservation and 386/64 hint/timer widths; content-sized pause/result panels including padding and borders; fractional collisions; native delta-time integration; original localized text; antialiased circular hold overlay using the required LVGL API 1.1 images, retained until Host stop; all exit paths committed before Host exit; 20-pixel accessory steps; firmware head-exit countdown cadence |
| Tetris v9 | 60x60 preview at (340,8), 10-pixel preview cells; 388-pixel instruction area; original 11-language text and dark instruction color; preview above instructions and hidden while looking up to exit; countdown width includes borders; committed head-exit after countdown; all 28 native rotation masks; native IMU debounce, 100ms simulation steps with fractional-time retention and clear animation; 32-bit score |

## Second source audit (2026-11-12)

Scope: the three native applications, their shared exit widget, UI adapter,
translation CSV and LVGL label/content-bound implementation. The reference is
the immutable commit above, not an assumption about which firmware is currently
installed on a user's glasses.

The audit found and corrected these additional port differences:

- Tetris I/S/Z 180-degree masks had been shifted down one row. All 28 masks
  now match the original matrices, including their rotation origins.
- Tetris previously accepted sustained gyro movement every 100ms. The native
  code samples every 100ms and requires strictly more than 100ms since the last
  accepted move (normally every other tick). Unrelated gyro quadrants no longer
  consume this debounce interval.
- Tetris previously advanced gravity and line clearing by the entire delayed
  callback duration. It now advances by one 100ms step, like the native timer.
  The exit countdown uses actual time independently of those simulation steps.
- Tetris countdown width omitted two border pixels; the preview also painted
  below the instructions. Its native 34px fixed badge height is retained.
- Tetris scores wrapped at 65,536. Both Tetris and Jet Runner now use 32-bit
  scores instead of 16-bit scores.
- Jet Runner discarded fractional positions before collision testing, delaying
  some edge collisions. Calibration also happened on the first frame instead
  of at restart. Both are corrected; a failed initial IMU read still defers
  calibration until a valid sample arrives.
- Jet Runner capped each callback at 100ms before accumulation, allowing a
  partially accumulated frame to exceed the limit. The final frame is capped.
- Breakout initially left the paddle and ball at default object positions until
  its first simulation frame. It now positions them immediately, including on
  restart. Its nearest trail now follows the previous ball position rather than
  overlapping the current position, and the final frame freezes as in firmware.
- Breakout rounded positions before brick/paddle collision checks and used
  multiple fixed substeps after a delay. It now retains fractional positions
  and applies the native elapsed-time scaling/smoothing, capped at 150ms.
- Breakout's score label had a fixed clipping width absent from the native
  implementation; it now sizes to its text.
- Breakout now returns immediately from its loop after requesting head-exit,
  avoiding accesses to plugin state if the Host unloads it synchronously.

Evidence: a direct snapshot comparison passed for 176 localized text values
across 11 languages and all 28 tetromino masks. The recording-Host regression
suite exercises fractional edge collisions, restart calibration, frame caps,
initial positions/trails, delayed-frame paddle motion, gyro cadence, line-clear
timing/collapse/scoring, game-over/restart, localized result/pause panels and
countdown widths. The new regression cases failed on the preceding ports and
passed after correction. Input and telephone-overlay fixtures also run against
the actual plugin sources. These are native C tests, not device or Studio
rendering tests; text widths are synthetic, with the firmware's 34px line height.

The hold overlay includes the original 18x18 `icon_delete.png`, an opaque
background, centered 48x48 ring and localized bottom text. It uses monotonic
time: step 1 after 800ms, step 2 at 1600ms, completion after 2000ms from LONG.
Release and suspend cancel an unfinished hold. Repeated LONG/VERY_LONG events
do not restart it. The game simulation continues beneath the overlay until the
hold completes, as in the reference.

On 2026-11-13, comparison of the native exit widget, application manager and
plugin Host identified a transition mismatch. Native code deletes the widget
then directly stops the game/starts the destination application. The GMP Host's
`app_exit()` queues `GM_PLUGIN_DISPLAY_REQUEST_STOP` for later processing. The
previous port hid its overlay before requesting exit, exposing a game frame
during that delay. Jet Runner v6 / Breakout v10 latch a committed exit state,
retain the opaque overlay until `on_stop` removes the UI, ignore queued input,
and stop simulation without sending duplicate exit requests. A new visible
cycle resets the latch. Regression checks reproduce the failure on the old
ports and verify delayed stop, release/click/BACK events, extra loops,
suspend/resume and subsequent restart on the corrected ports. This does not
establish a device recording of the final transition.

The follow-up exit audit found two uncovered paths: Jet Runner BACK did not
set the committed state, and Tetris head-exit could request exit every second
while continuing simulation between requests. Jet Runner v7 / Tetris v8 now
commit those paths before `app_exit()`. Until Host stop, further loops, queued
buttons and suspend/resume neither advance the game nor write game UI or use
IMU services. A new visible cycle restores normal simulation and controls.
The recording-Host regression covers normal play, BACK during an unfinished
hold, Tetris line clearing, and game-over restart input. The countdown remains
cancellable before completion, with its existing cadence and simulation.

Breakout v11 extends committed exits to head-exit, BACK and restart failure.
Its earlier `exiting == 2` guard stopped physics but still allowed cooked clicks
to write UI and suspend/resume to reactivate callbacks. Regression now covers
BACK/head-exit during play, pause and game over, BACK during a hold, failed
restart, and raw input after a pending exit.

Tetris v9 retains the remainder below 100ms across callbacks. Clearing the
accumulator previously made 16ms callbacks advance a step every 112ms, slowing
the game systematically. Fixed 16/33/60ms and jittered callback tests check
gravity, fast drop and line clearing. Delayed callbacks still advance at most
one step and discard missed whole steps, without accumulating catch-up work.

After the source audit, visual feedback from both Studio and glasses exposed
visible polygonal corners in the polyline exit ring. Breakout v9 / Jet Runner
v5 replace it with a supersampled analytic annulus: 48px diameter, 4px stroke,
16 alpha levels, clockwise half-circle with rounded caps, then a full circle.
`common/generate_exit_art.py` produces the original icon and both ring states
as aligned indexed images. Jet Runner v8 / Breakout v12 require the plugin LVGL
API 1.1 and its `image_create` entry. The former API 1.0 fallback, which created
682 pixel-run objects in addition to containers and text, has been removed
along with its generated run tables. The exit UI now always has five objects:
one overlay, one label and three images. Version/table-size/function checks
reject incompatible Hosts at entry, before UI creation. The recording-Host
tests verify the five-object budget across repeated game restarts and cleanup
after allocation failures. The underlying firmware LVGL library version is
separate from this plugin interface requirement.

Regenerate with Python and Pillow. Runtime needs neither dependency.
`tests/test_exit_art.py` checks radial coverage, symmetry, stroke bounds and
the unchanged image payloads. The generated image pixels were visually
inspected enlarged before removal of the fallback; this is not a claim of
updated Studio/device validation.

## Deliberate adaptations and verification limits

- The public LVGL table has no `arc_set_bg_angles` or `arc_set_rotation`.
  `common/firmware_exit.h` uses antialiased circular images instead. Its edges
  are no longer polyline approximations, but exact equality to native LVGL's
  arc rasterizer is still not asserted.
- The native hold overlay uses the default container theme. The port specifies
  its black background and padding explicitly to keep Host/Studio themes from
  changing its layout. The bottom text uses a 16-pixel inset.
- Layout uses Host-reported dimensions. The native reference is 600x350; other
  sizes retain the ports' adaptation and cannot be called pixel-identical.
- The referenced firmware maps `font_default` to `lv_font_xgimi_17`, matching
  the native games. That font has a 34px line height despite its name. Studio or
  another Host may supply different fonts. Glyph rasterization and clipping at
  the native fixed-height countdown badge still need visual verification.
- The existing fixed-point physics, SDK lifecycle, optional HOGP input and
  telephone UI cooperation remain. Tetris keeps accessory Up/Down actions
  added by the GMP port. This is not a byte-for-byte copy of firmware logic.
- Jet Runner/Breakout positions and velocities use 1/256-pixel fixed point;
  firmware uses floats. Rounding can accumulate over a long run even though
  collision equations and frame scaling now follow the original. Random seeds
  depend on startup timing; Tetris uses a local generator instead of firmware
  `rand()`, so piece sequences are not expected to match round-for-round.
- Breakout limits message panels to the display with side margins. The native
  labels are content-sized without this explicit limit; unusually wide fonts
  or smaller displays can therefore wrap differently. The port also keeps its
  richer mouse/touch/gamepad controls and rejects unsupported display sizes.
- Host callback and input event scheduling differs from native LVGL timers.
  Head-exit display updates may differ by a callback interval; stalled-loop
  countdown behavior is not guaranteed identical. The hold overlay ignores
  repeated LONG events and accepts VERY_LONG without restarting its timer.
- Native Tetris/Breakout text says three seconds to exit, but their code waits
  one second before displaying 3, then 2, then 1, then exits. This cadence is
  preserved; it is about four seconds from head-raise detection.

## Checks

From the repository root, with a native C compiler:

```sh
CC=gcc CFLAGS="-fsanitize=address,undefined -g" python3 GlassSDK/tests/test_game_parity.py
CC=gcc CFLAGS="-fsanitize=address,undefined -g" python3 GlassSDK/tests/test_input_native.py
```

The parity fixture calls the actual game entry points with a recording Host.
It checks layout, text, hold timing after slow frames, release cancellation,
BACK, input, suspend/resume, stop/restart, failed creation, denied IMU,
unsupported displays, telephone UI and Hosts without optional system events.
It does not render actual LVGL pixels or validate Bluetooth radio delivery.

Build only these examples from `GlassSDK/`:

```powershell
py build.py build --example game/jet_runner
py build.py build --example game/breakout
py build.py build --example game/tetris
```

Keep the `.gmp`, `.review.json` and `.review-source.enc` from each successful
build together. Final acceptance still needs native-versus-plugin screenshots
and button/IMU checks on the same glasses firmware, display size and locale.
