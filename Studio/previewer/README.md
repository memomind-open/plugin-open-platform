# GM Plugin Previewer

GM Plugin Previewer is a Windows desktop preview tool that does not require
physical glasses. It loads `.gmp` files produced by the SDK, executes their
RV32 plugin code in software, and simulates the firmware Host API, LVGL drawing
API, GRAY_4 framebuffer, and input devices.

It has two goals:

- Functional validation: verify `.gmp` format checks, relocation, entry-point
  checks, and lifecycle calls, and test button, IMU, Bluetooth, and device-state
  logic.
- Approximate visual validation: preview UI at the glasses' 600×350 logical
  resolution with 4-bit grayscale and LVGL 8.3 API semantics.

The Previewer directly uses the firmware `lv_font_xgimi_17/20` fonts, so glyph
shape, advance, baseline, and line height come from device resources. It is
still not an optical simulation of physical glasses. Validate LVGL theme
details, display brightness, optical projection, refresh timing, IMU noise, and
Bluetooth behavior on the device.

## Run directly

The built Windows x64 application is located at:

```text
Studio/.build/previewer/windows-mingw-x64/GMPluginPreviewer.exe
```

After launch, you can:

1. Select **Open .gmp** or drag an `.gmp` file into the window.
2. Inspect the 600×350 green monochrome preview.
3. Use **Raise / Lower / Turn left / Turn right** to adjust the simulated head
   pose in 3° steps.
4. Use **Nod / Left / Right / Shake** to send recognized firmware IMU gestures.
5. Send single-click, double-click, and long-press button events.
6. Change simulated battery, wear, charging, three-axis gyroscope, and pitch
   state.
7. Send a simulated phone Bluetooth message to the plugin on a chosen channel.
8. Inspect plugin logs, Host logs, and Bluetooth messages sent by the plugin.

Head-pose steps and gesture events are independent test paths. Head-pose steps
change the raw data returned by `imu_read()`, while **Nod / Left / Right /
Shake** directly test the plugin's `on_event`. Raising and lowering the head
persistently change the absolute `pitch_degrees` supplied by the Host API. The
current Host API has no absolute yaw field, so left and right angles appear only
in the Previewer; the plugin receives a short three-axis gyro motion sequence.

You may also pass an `.gmp` on the command line:

```powershell
.\Studio\.build\previewer\windows-mingw-x64\GMPluginPreviewer.exe C:\plugins\my-plugin.gmp
```

## Build

The current development environment can cross-compile the Windows executable
on Linux:

```bash
Studio/scripts/build-previewer-windows.sh
```

To use firmware fonts, specify the font directory when configuring CMake:

```powershell
cmake -S Studio/previewer \
  -B Studio/.build/previewer/windows-mingw-x64 \
  -DGM_PREVIEW_FONT_DIR=C:\path\to\fonts
```

The directory must contain:

```text
fonts/
├── lv_font_xgimi_17.bin
└── lv_font_xgimi_20.bin
```

The fonts are compiled into the executable and do not need to be distributed as
separate runtime files.

Build the headless validation tool on Linux with:

```bash
Studio/scripts/build-previewer.sh
```

CLI examples:

```bash
Studio/.build/previewer/linux-release/gmplugin-preview-cli \
  GlassSDK/build-host/lvgl_ui/lvgl_ui.gmp \
  --frames 30 --pgm /tmp/lvgl-ui.pgm

Studio/.build/previewer/linux-release/gmplugin-preview-cli \
  GlassSDK/build-host/bluetooth/bluetooth.gmp \
  --bt 1 "Hello plugin"
```

## Implemented capabilities

### `.gmp` loading and execution

- GMP v1 magic, version, size, ABI, and CRC32 validation.
- Image copy, BSS clearing, and base relocation matching current firmware.
- Plugin entry point, descriptor, and callback address-range validation.
- `on_load`, `on_start`, `on_resume`, `on_loop`, `on_event`, `on_suspend`,
  `on_stop`, and `on_unload` lifecycle callbacks.
- Interpreted RV32I, M, A, F, and C instructions with guest-memory bounds and a
  per-callback instruction budget.
- Plugin binaries are never executed directly as native x86 code.

### Host API simulation

- Logging, monotonic time, Host heap allocation, and leak reporting.
- 600×350, 30 Hz, GRAY_4 display information.
- Direct framebuffer lock/unlock over two synchronized 175-row regions.
- Display power, brightness, optical distance, height, and automatic-brightness
  blocking.
- Simulated Bluetooth send and receive.
- IMU enable/read, configurable raw samples, 3° head-pose steps, and gesture
  events.
- Battery, charging, wear state, locale, connection, and App exit.
- Demo and LZ4 extensions.

### LVGL compatibility layer

The complete public `gm_plugin_lvgl_api_t` 1.0 function table is implemented,
including:

- root, generic object, label, arc, and line objects;
- position, size, alignment, flags, and common styles;
- text measurement, wrapping, and 2 bpp glyph rendering with firmware XBF
  fonts;
- a 34-pixel line height and 7-pixel baseline for the default font, and a
  41-pixel line height and 9-pixel baseline for the large font; and
- approximate rendering of common background, border, text, line, and arc
  styles.

The Previewer simulates the public SDK LVGL API semantics; it does not copy the
entire firmware LVGL binary into the PC application. UI layout and interaction
are suitable for development preview, but pixel-level screenshots are not an
acceptance standard for physical glasses.

## Regression validation

Run:

```bash
Studio/previewer/tests/smoke.sh
```

The regression script runs every maintained example in `GlassSDK/build-host`
and additionally verifies that:

- a Bluetooth message enters the plugin and produces a reply;
- button and IMU gesture events enter the plugin;
- a directional gesture produces both one recognized event and a returning Raw
  IMU trajectory, supporting device plugins that either deduplicate firmware
  direction events or detect direction from Raw IMU; and
- the framebuffer example produces a non-empty PGM image.

The implementation has passed these 15 examples:

```text
minimal       input          imu             device_state
bluetooth     extension      lz4             framebuffer
lvgl_ui       scene_bridge   2048            breakout
jet_runner    snake          tetris
```

## Source layout

```text
previewer/
├── core/                RV32 executor, GMP loader, Host API, and rendering
├── include/             Public C and C++ previewer interfaces
├── apps/cli/            Headless validation frontend
├── apps/win32/          Standalone Windows compatibility frontend
├── resources/           Native frontend resources
├── tests/               Core integration and SDK regression tests
└── third_party/         Pinned LVGL compatibility dependency
```

Build scripts and generated files live outside the source tree under
`Studio/scripts/` and `Studio/.build/`, respectively.

## Current limitations

- Glyph bitmaps and font metrics come from firmware, but LVGL themes, object
  rendering, blending, and clipping are compatibility implementations; a full
  frame is not guaranteed to match pixel for pixel.
- F-extension rounding and floating-point exception flags use approximate Host
  semantics. Maintained SDK examples primarily use integer paths.
- Simulated Bluetooth records outbound plugin packets but does not connect to a
  real phone.
- The IMU provides deterministic configurable samples and does not simulate
  sensor noise or sampling latency.
- SPI, optical projection, firmware task scheduling, memory pressure, and real
  performance are not simulated.
- An `.gmp` remains untrusted input. Guest instructions and memory are not
  directly mapped to native execution, but parser fuzzing, resource limits, and
  a security audit are still required before public distribution.
