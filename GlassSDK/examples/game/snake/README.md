# Snake plugin

An IMU-controlled Snake game built with the core LVGL plugin API.

- Move your head up, down, left, or right to select the absolute snake heading.
  Only the initial motion away from center is accepted. The opposite IMU peak
  while returning to center rearms the recognizer without issuing a command.
- Eat the bright food square to grow, score points and gradually speed up.
- A single click restarts after game over; a long press exits at any time.
- Reversing directly into the snake body is filtered out.

After changing this example, run `./gm-build` from the SDK root. The output is
`build-host/game/snake/snake.gmp` and the command displays its QR code.
