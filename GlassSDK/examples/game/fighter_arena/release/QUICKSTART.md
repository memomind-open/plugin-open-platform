# Fighter Arena Beta Quick Start

Version: `0.1.0-beta.22`
Plugin manifest version: `30`

## Requirements

- A complete `plugin-open-platform` release checkout
- MemoMind Plugin Studio for Windows x64, Linux x64, or macOS Universal 2
- `fighter-controller.mmpkg`
- `GlassSDK/build-host/.build/game/fighter_arena/fighter_arena.gmp`

Desktop Studio is distributed as a prebuilt application under
`Studio/<platform>/`. Its source repository is not part of the public SDK.

## Preview in Desktop Studio

1. Start MemoMind Plugin Studio from `Studio/<platform>/`.
2. Select or import `fighter-controller.mmpkg` as the Web plugin.
3. Select or import
   `GlassSDK/build-host/.build/game/fighter_arena/fighter_arena.gmp` as the
   glasses plugin.
4. Start the pair and keep the Web plugin panel focused while using keyboard
   controls.
5. Use the controller UI or mapped keyboard controls to move, jump, crouch,
   punch, kick, trigger skills, pause, and resume.

Desktop Studio runs both packages in one process and routes their custom
`plugin.sendMessage` channels without a Bluetooth connection.

## Run on physical glasses

1. Serve `GlassSDK/build-host/.build/game/fighter_arena/fighter_arena.gmp`
   with the GlassSDK QR installation workflow and scan it from the official
   App's device-plugin debug page.
2. Import or launch `fighter-controller.mmpkg` in an official App version that
   supports Web plugins.
3. Connect the App to the glasses and start both plugins.
4. Keep the Fighter Controller Web plugin active while playing.

See [`../../../../docs/INSTALLATION.md`](../../../../docs/INSTALLATION.md) for
the current GMP installation workflow and
[`../../../../../WebSDK/docs/web-plugin/quick-start.md`](../../../../../WebSDK/docs/web-plugin/quick-start.md)
for Web plugin packaging and App delivery.

## Controls

- Move left or right, jump, and crouch with the matching direction controls.
- Use light/heavy punch and light/heavy kick for normal attacks.
- Use the energy-wave and tracking-rush controls for special attacks.
- Use pause to suspend or resume the match.

Blocking has no separate `GD` meter or guard break. Holding away retreats until
an incoming attack enters its active range, when the same input becomes a
stand block. Combine away with down for a crouch block.

Audio is mixed by the Fighter Controller Web plugin on the phone or computer,
not by the glasses. Losing the Web plugin connection pauses the match and its
music.
