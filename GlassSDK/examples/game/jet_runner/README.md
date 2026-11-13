# Jet Runner plugin

Standalone fixed-point port of the firmware Jet Runner. Its layout, pixel-art
pilot, obstacles, collision box, speed curve and localized text follow the
built-in game while avoiding floating point, linked libc and plugin allocation.
See [firmware parity](../FIRMWARE_PARITY.md) for the source revision and the
remaining rendering differences that need device validation.

Version 4 keeps fractional coordinates for collision checks, captures neutral
pitch when restarting, and caps the accumulated frame duration at 100ms.
Scores use 32 bits, matching the native game's practical range.

Version 5 replaces the visibly polygonal hold-exit ring with antialiased circular
image states.

Version 6 retains the exit overlay after hold completion until the asynchronous
Host stop arrives. Queued input and loop callbacks cannot reveal the game again.

Version 7 also commits BACK exits before requesting Host stop. Simulation and
input stay frozen during the wait, including suspend/resume and repeated BACK.

Version 8 requires the plugin LVGL API 1.1 image interface. The exit UI uses
five objects: one overlay, one label and three images. The API 1.0 pixel-object
fallback has been removed; incompatible Hosts are rejected before UI creation.
This requirement is the plugin interface version, not the LVGL library version.

- Look up/down or press the accessory up/down buttons: fly
- Hold an accessory up/down button to keep moving in that direction
- Single/double click after collision: restart
- Long-press: show the firmware-style exit overlay; release cancels it.
  The ring advances at about 0.8/1.6 seconds and exits after 2 seconds from
  the LONG event (the Host's initial long-press recognition time is additional).
- Accessory BACK: exit immediately
- Supports all 11 Host languages, with English fallback
