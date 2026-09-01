# Fighter Arena Controller

This Web plugin is a multi-touch controller for the Fighter Arena GMP game.
It sends Fighter Arena version 2 state snapshots every 50 ms.
The controller is landscape-only. Because the production App currently keeps
its WebView in portrait, the plugin rotates its complete canvas by 90 degrees
on portrait touch devices and swaps the viewport dimensions. The user can hold
the phone horizontally without requiring native orientation support.
Movement uses a virtual analog joystick with a center deadzone. It maps to the
same digital left/right/up/down bits expected by the game and supports diagonal
bit combinations.
The four attack buttons use a separated gamepad diamond: `I` guard at the top,
`U` kick on the left, `K` heavy attack on the right, and `J` light attack at
the bottom.

- Input channel: `0x4647`
- Payload: `[version, sequence, buttons_hi, buttons_lo]`
- Bridge permission: `device.events` for connection-state subscription;
  plugin messaging itself is available by default

Input confirmation sounds play locally for attack, guard, skill, start and
pause controls. The controller also consumes the glasses plugin's four-byte
channel `0x4648` event frames through `gm.plugin.onMessage()`. It plays combat
impacts, successful blocks, guard breaks, special launches, jumps, KO,
round-start and round-result cues, menus, and five music states. Matching local
and game-confirmed attack sounds are deduplicated within 350 ms.

The event payload is `[version, sequence, event, value]`, where version is `1`.
Events `1..11` are hit, block, guard-break, special-launch, round-end, attack,
jump, round-start, menu, KO, and music. Generated audio assets are documented
under `assets/sfx/README.md`.

Run it in WebSDK Browser Studio for Web-only protocol checks:

```sh
node tools/run-browser-studio.mjs --plugin examples/fighter-controller
```

Build the package:

```sh
npm run pack:plugin -- examples/fighter-controller dist/fighter-controller.mmpkg
```

Browser Studio validates and records the outgoing messages. Use Desktop Studio
to run this Web plugin together with `fighter_arena.gmp`. Real glasses control also
requires the App WebView host to implement `plugin.sendMessage` by forwarding
the channel and decoded payload through GM service `0x0F`, command `0x28`.
The App must route unsolicited command `0x29` messages to the WebView event
bridge as `plugin.message` rather than its `0x28` command-response queue.
