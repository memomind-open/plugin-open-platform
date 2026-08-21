# Fighter Arena Controller

This Web plugin is a multi-touch controller for the Fighter Arena GMP game.
It sends the same version 2 state snapshots as GMPluginWindows every 50 ms.
The controller is landscape-only. Because the production App currently keeps
its WebView in portrait, the plugin rotates its complete canvas by 90 degrees
on portrait touch devices and swaps the viewport dimensions. The user can hold
the phone horizontally without requiring native orientation support.
Movement uses a virtual analog joystick with a center deadzone. It maps to the
same digital left/right/up/down bits expected by the game and supports diagonal
bit combinations.
The four attack buttons use a separated gamepad diamond: `I` guard at the top,
`J` light attack on the left, `K` heavy attack on the right, and `U` kick at
the bottom.

- Input channel: `0x4647`
- Payload: `[version, sequence, buttons_hi, buttons_lo]`
- Bridge permission: `device.events` for connection-state subscription;
  plugin messaging itself is available by default

Input confirmation sounds from GMPluginWindows play locally for attack, guard,
skill, start and pause controls. Hit, successful block, KO, round-result and
background music remain disabled until the App Bridge forwards channel
`0x4648` game events from the glasses.

Run it in Studio:

```sh
node tools/studio-cli.mjs --plugin plugins/fighter-controller
```

Build the package:

```sh
npm run pack:plugin -- plugins/fighter-controller dist/fighter-controller.mmpkg
```

Studio validates and records the outgoing messages. Real glasses control also
requires the App WebView host to implement `plugin.sendMessage` by forwarding
the channel and decoded payload through GM service `0x0F`, command `0x28`.
