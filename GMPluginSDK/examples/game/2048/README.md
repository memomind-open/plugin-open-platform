# 2048 game

This example implements the classic 4 x 4 2048 puzzle with the shared Host
LVGL table. Tilt or turn your head up, down, left, or right to slide the board.
Raw IMU input treats moving away and returning to a stable neutral pose as one
complete action. The recorded move is applied only after that return, so the
board refresh happens while the player's head is centered.
Equal adjacent tiles merge once per move, and every effective move creates a
new 2 or 4 tile. The current score is the sum of all merged tile values.

When no legal move remains, click to restart. Long-click the button to exit.

After changing this example, run `./gm-build` from the SDK root. The output is
`build-host/game/2048/2048.gmp` and the command displays its QR code.
