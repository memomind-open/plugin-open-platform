# Display Control Lab glasses plugin

This example demonstrates the frozen `display_control` Host API with a paired
phone Web plugin. It supports display power, brightness levels 1-10, optical
distance levels 0-8, vertical display-height levels 0-8, and restoring the
values captured during `on_start`.

Manual brightness temporarily blocks automatic brightness. The block is
released by Restore, long-press recovery, and `on_stop`. The Host remains
responsible for restoring every control changed by the plugin when it stops.

The plugin enables firmware gesture events. While the display is off, any
glasses or accessory button and an active head-raise gesture turn it on. The
updated power state is sent to the paired Web plugin without waiting for a new
phone command.

Desktop Studio receives normal display-control calls for brightness, height,
and distance. Screen-power commands can carry a `previewOnly` flag, allowing
the Studio display to remain visible while the full phone/GMP command path is
tested. Physical-device commands do not set that flag.

Safety behavior: while the display is on, holding the primary button or
triggering Back requests the screen to remain on, releases automatic-brightness
blocking, and exits the demo. While it is off, the first button event only wakes
the display and is consumed.

Build from `GlassSDK`:

```sh
python3 build.py build --example display_control_lab
```

On Windows PowerShell, replace `python3` with `py`.
