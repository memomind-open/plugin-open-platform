# Jet Runner plugin

Standalone fixed-point port of the firmware Jet Runner. Its layout, pixel-art
pilot, obstacles, collision box, speed curve and localized text match the
built-in game while avoiding floating point, libc and plugin allocation.

- Look up/down: fly
- Single/double click after collision: restart
- Hold for 2 seconds: exit
- Supports all 11 Host languages, with English fallback

After changing this example, run `./gm-build` from the SDK root. The package is
written to `build-host/game/jet_runner/jet_runner.gmp` and served through the
QR flow.
