# Display Control Lab Web plugin

Display Control Lab 0.1.1 is the phone-side controller for the paired
`display_control_lab.gmp`. The Web plugin does not expose privileged optical
controls directly. It sends bounded binary commands to the glasses plugin,
which validates and applies them through the frozen GlassSDK display-control
Host API.

The bilingual UI provides:

- display power on/off;
- brightness levels 1 through 10;
- vertical display-height levels 0 through 8;
- optical-distance levels 0 through 8; and
- restoration of the values captured when the GMP started.

Range controls send one command on release instead of sending every slider
movement over Bluetooth. Every command carries a request ID and receives a
complete state response with a result code.

## Simulator behavior

PhoneSDK Browser Studio applies all controls to a local preview model. Desktop
Studio runs the real GMP and Host API simulation, but screen-power commands are
marked `previewOnly` so the virtual glasses UI remains visible. Brightness,
height, and distance values are validated and reflected without attempting to
simulate optical effects.

On physical glasses, screen-power commands are applied normally. If the display
is turned off, any glasses or accessory button and an active head-raise gesture
turn it on and immediately synchronize the new power state to the phone page.
Holding the primary glasses button while the display is on releases the
automatic-brightness block and exits the demo.

## Build

From `PhoneSDK`:

```sh
node tools/build-mmpkg.mjs examples/display-control-lab dist/display-control-lab-0.1.1.mmpkg
```

Pair it with:

```text
GlassSDK/build-host/.build/display_control_lab/display_control_lab.gmp
```
