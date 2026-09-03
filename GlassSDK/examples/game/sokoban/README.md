# Sokoban plugin

A head-controlled Sokoban puzzle built with the core LVGL plugin API.

- Move your head up, down, left, or right to move one tile. As in the Snake
  example, the recognizer waits for the return motion and a quiet center pose
  before accepting the next command.
- Press the accessory direction buttons as an alternative control method.
- Push every box onto a marked goal to finish each of the 36 solver-verified
  levels. Level 1 is an empty-room tutorial with the player lined up behind a
  centered box like a billiards break. The box has open floor on all four sides,
  so the player can circle it and learn horizontal and vertical pushes while
  moving it into the upper-right goal. The remaining campaign starts at 17
  minimum pushes and rises to 30, with 3--5 boxes, tighter routes and
  order-dependent pushes.
- The player and crates use shared 16 x 16 pixel-art resources. Each level
  stores only its wall, goal, crate and starting-player layout.
- Single-click to undo a move. Double-click to restart the current level.
- After clearing a level, single-click to continue. Hold for two seconds to
  exit at any time.

Build only this example from the GlassSDK root:

```sh
python3 build.py build --example game/sokoban
```
