# GM Plugin examples

Examples progress from focused Host modules to complete applications. Edit one
example, then run `python3 build.py` from the SDK root. CMake and
Ninja rebuild the affected source files, create
`build-host/.build/<relative-path>/<example>.gmp`, then exit. A copied example
may use any nesting depth but must remain below the
SDK's `examples/` directory so the build driver and Desktop Studio can discover
it. Source files can use any descriptive `.c` name; `plugin.c` is not a required
convention. Do not add an example-local `CMakeLists.txt`: the internal build
definition discovers every `manifest.json` and compiles all `.c` files in its
example directory.

After the build completes, refresh Desktop Studio to select, simulate, package,
share, or generate a QR code for the resulting `.gmp`.

Display geometry is a runtime capability. Examples that size or position a
screen-level layout call `host->display_get_info()` instead of assuming a fixed
panel resolution. Constants that remain in game examples describe game objects,
spacing, or minimum usable layout sizes rather than device width or height.

| Example | What it teaches | Required capability |
| --- | --- | --- |
| `extension` | query, validate and call `extension_get` function tables | LZ4 + libc extensions |
| `lvgl_ui` | core `host->graphics.lvgl` drawing, text and `on_loop` | none (core LVGL) |
| `framebuffer` | zero-copy lock/draw/unlock across Host framebuffer slices | display bitmap |
| `input` | button events and active exit | button |
| `imu` | gesture events and pull-based raw IMU | IMU events/raw + libc extension |
| `bluetooth` | bidirectional channel + byte messages | Bluetooth + libc extension |
| `web_bridge` | WebView-driven Scene rendering plus button/IMU uplink and framed LZ4 transfer | display bitmap + Bluetooth + button + IMU + libc; optional LZ4 |
| `audio_capture_lab` | lightweight foreground/status companion for the Web Audio Capture Lab | Bluetooth + button + libc extension |
| `talking_pet` | native animated companion paired with the phone-side Talking Pet | display bitmap + Bluetooth + button + libc extension |
| `game/breakout` | complete local game | button + raw IMU + locale + libc extension |
| `game/tetris` | grid game with IMU movement and button rotation | button + raw IMU + locale + libc extension |
| `game/jet_runner` | scrolling IMU-controlled runner | button + raw IMU + locale + libc extension |
| `game/snake` | IMU-controlled snake game | button + raw IMU + libc extension |
| `game/sokoban` | 36-level box-pushing puzzle with head-motion controls, shared pixel art and undo | button + raw IMU + libc extension |
| `game/2048` | 4 x 4 number-merging puzzle with accessory navigation | button + libc extension |
| `game/fighter_arena` | fixed two-fighter best-of-three match with specials and character AI | button + Bluetooth + libc extension |

Build the changed module from the SDK root:

```sh
python3 build.py
```

On Windows, use Windows PowerShell or Terminal (PowerShell) and run
`py build.py`. Do not use Command Prompt.

Build every maintained example:

```sh
python3 build.py all
```

Build only one example by passing its path from this table:

```sh
python3 build.py build --example game/2048
```

On Windows, replace `python3` with `py`.

Audio is intentionally outside the GMP ABI. Web plugins request Host-owned
glasses capture and playback through the PhoneSDK `gm.audio` API. The
`audio_capture_lab` GMP receives only low-rate UI state over Bluetooth; audio
uses the dedicated Host/Web binary stream and never passes through the GMP.
