# Snake plugin

An IMU-controlled Snake game built with the core LVGL plugin API.

- Move your head up, down, left, or right to select the absolute snake heading.
  Only the initial motion away from center is accepted. The opposite IMU peak
  while returning to center rearms the recognizer without issuing a command.
- Press the accessory up, down, left, or right button to select the snake
  heading. Direct reversals and additional turns before the next step are
  ignored, matching the head-motion controls.
- Eat the bright food square to grow, score points and gradually speed up.
- A single click restarts after game over. A long press shows a two-second exit
  countdown; release the button to cancel it.
- Reversing directly into the snake body is filtered out.
