# 2048 game

This example implements the classic 4 x 4 2048 puzzle with the shared Host
LVGL table. Use the Bluetooth accessory Up, Down, Left, and Right navigation
buttons to slide the board. Each `TRIGGER` event applies one move immediately;
holding a repeatable direction can generate repeated moves.
Equal adjacent tiles merge once per move, and every effective move creates a
new 2 or 4 tile. The current score is the sum of all merged tile values.

When no legal move remains, click to restart. Long-click the button to exit.

After changing this example, run `./gm-build` from the SDK root. The output is
`build-host/game/2048/2048.gmp` and the command displays its QR code.
