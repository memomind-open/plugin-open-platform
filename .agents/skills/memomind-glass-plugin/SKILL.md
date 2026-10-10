---
name: memomind-glass-plugin
description: Develop or fix MemoMind GlassSDK native C plugins, Host ABI integration, rendering, input, and lifecycle behavior. Use for glasses .gmp implementations, not independent Bluetooth clients or private Desktop Studio source changes.
---

# Native glasses plugins

Document paths below are relative to this file; command working directories are stated explicitly. Start with the [GlassSDK guide](../../../GlassSDK/README.md) and [ABI](../../../GlassSDK/docs/ABI.md), and check the relevant public headers in `GlassSDK/include/`. Load additional documents by feature instead of duplicating the ABI manual in the skill.

## Choosing references

- Rendering: read [GRAPHICS](../../../GlassSDK/docs/GRAPHICS.md); use framebuffer, lvgl_ui, or image_animation as references.
- Input and IMU: read [INPUT](../../../GlassSDK/docs/INPUT.md); refer to input and imu.
- Bluetooth messages: refer to bluetooth, web_bridge, and [protocol compatibility](../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md). Check fields, lengths, channels, and versions on both sender and receiver.
- Capabilities and extensions: read the [capability matrix](../../../GlassSDK/docs/CAPABILITY_MATRIX.md). Validate the ABI, table size, version, and function pointers actually used; a manifest does not establish firmware capability.
- Display plugins: read [SYSTEM_EVENTS](../../../GlassSDK/docs/SYSTEM_EVENTS.md). Refer to `examples/common/call_ui.h` and fighter_arena's direct framebuffer policy.

## Starting a new application

Use the [application skill](../memomind-example-app/SKILL.md) to choose a reference or generate a minimal project. Query display dimensions through `host->display_get_info()` instead of treating Studio's 600x350 profile as a fixed capability of every firmware version.

Give a copied example an independent manifest identity. Check includes for common/call_ui.h and assets shared with other examples; adjust relative paths when directory depth changes. The minimal generator copies call_ui.h into the project for standalone builds. For images, IMU, and game state, reuse the actual APIs and asset formats of the corresponding examples.

## Implementation constraints

Export only `gm_plugin_entry` and access firmware services through the Host table. The entry point validates the Host and fills the descriptor without allocating resources. `on_load` does not create UI; `on_start` begins a visible cycle. Failed load/start callbacks must clean up partial resources themselves rather than relying on unload/stop callbacks that will not run.

Callbacks execute on the display task and must not block, sleep, or spin. Borrowed event data is valid only during its callback. Plugins must not delete the Host's LVGL root. Read raw IMU data through the public pull interface.

When telephone UI appears, yield pixels in every drawing path, including drawing triggered by buttons. Keep loops, Bluetooth, input, and protocol ACKs active. Schedule restored drawing for the next loop. Provide a reasonable fallback when older firmware lacks the extension, and do not claim that the current Studio simulates this event.

Preserve the frozen core ABI. Use a separate extension ID for incompatible extension changes rather than altering published table layouts. Do not directly link firmware libraries, LVGL, libc, or FreeRTOS.

Memory and stack limits come from the ABI and build tools: Flash <=512000 B, static RAM <102400 B, and each static function stack frame <=1024 B. These checks do not prove total call-stack or dynamic-heap safety. Avoid large local arrays, VLAs, and unbudgeted dynamic allocations.

## Companion and Studio selection

State whether this is glasses-only or part of a paired application. A standalone program needs no phone plugin; instruct the developer to disable/clear the phone component in Studio. A custom paired application needs its actual phone implementation, matching manifest requirements, implemented message handlers, and both built packages. Use the [pairing preflight and handoff](../memomind-example-app/references/paired-app.md) to check the intended pair. A generated LVGL starter does not automatically implement Web Bridge protocols or a phone companion.

## Verification

Build a single example from `GlassSDK/`, such as `python3 build.py build --example game/2048`. New plugins follow the existing manifest/C source layout without adding an example-local CMakeLists.

Select unittest cases under `tests/` according to impact. For telephone UI, use `python3 -m unittest discover -s tests -p test_system_native.py`; for input, use `test_input_native.py`. For packaging, stack, and image layout changes, inspect the relevant tests and compiler requirements. Report skips and missing dependencies; a skipped test does not establish coverage.

Read the delivery skill when producing a `.gmp`. Explain simulator coverage and the display, input, communication, and firmware compatibility behavior still requiring physical-device checks.
