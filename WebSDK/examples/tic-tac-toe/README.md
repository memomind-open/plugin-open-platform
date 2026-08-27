# Aphrodite Chinese Tic-Tac-Toe Plugin

This standalone TypeScript/Vite Web plugin communicates with the App through
Aphrodite Bridge v1. It does not access Bluetooth directly; the App forwards
device drawing and button/IMU events.

## Gameplay

- The player uses X, the computer uses O, and the player moves first.
- Move the head up or down to select an empty cell in the same column.
- Turn left or right to select an empty cell in the same row.
- Single-click the primary button to place a mark; double-click to restart.
- At the end of a game, the Web page and device clearly report a win, loss, or
  draw.
- The controls and board on the Web page can also be used directly.

## Device display

- The device uses a 576×288 landscape layout.
- The left side contains Chinese rules, controls, and current status.
- The right side contains a 256×256 GRAY_4 board with X/O marks, the current
  selection, and the winning line.
- The Chinese rules on the left are updated first, and the 256×256 board on the
  right is submitted as the final drawing operation so later text does not
  overwrite the board refresh.
- The Web plugin encodes the complete GRAY_4 board as a strict raw LZ4 block and
  sends it through `display.updateImageLz4`. If compression does not reduce its
  size, it falls back to one complete uncompressed image instead of four tiles,
  avoiding a damaged image after a partial tile failure.
- High-frequency operations use latest-wins scheduling: at most one board is in
  flight and one newest board waits to be sent, preventing old frames from
  accumulating and timing out in the Bluetooth queue.

## Development

```bash
# Run from examples/tic-tac-toe.
npm ci
npm test
npm run typecheck
npm run build
```

Build output is written to `dist/`. `vite.config.ts` uses relative asset paths,
so Aphrodite's local plugin resource server can load the output directly.

Validate the build in WebSDK Browser Studio:

```bash
# Run from the WebSDK workspace root.
node tools/run-browser-studio.mjs --plugin examples/tic-tac-toe/dist
```

Package the plugin:

```bash
npm run pack:plugin -- examples/tic-tac-toe/dist dist/tic-tac-toe.mmpkg
```

## App integration

Copy the complete production build into Aphrodite's
`assets/plugin_tictactoe/`. Do not copy source files, tests, Node dependencies,
or `node_modules` into the App repository.

Running on physical glasses requires the unified device plugin that supports
Scene channels, Ping, button events, and IMU events.
