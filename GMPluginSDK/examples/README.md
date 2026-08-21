# GM Plugin examples

Examples are ordered from the smallest ABI skeleton through one Host module at
a time. Edit one example, then run `./gm-build` from the SDK root. CMake and
Ninja rebuild the affected source files, create
`build-host/<example>/<example>.gmp`, and starts the QR installation server. A
copied example must remain below the SDK's `examples/` directory so the driver
can discover it. Source files can use any descriptive `.c` name; `plugin.c` is
not a required convention. Do not add an example-local `CMakeLists.txt`: the
SDK-root build file discovers every `manifest.json` and compiles all `.c` files
in its example directory.

The QR package used after the first full build or when nothing changed is set
by `[serve] default_example` in the SDK-root `gm-build.ini`; it is `lvgl_ui` by
default. Set it to an `examples/`-relative directory, without a `.gmp` suffix.

Display geometry is a runtime capability. Examples that size or position a
screen-level layout call `host->display_get_info()` instead of assuming a fixed
panel resolution. Constants that remain in game examples describe game objects,
spacing, or minimum usable layout sizes rather than device width or height.

| Example | What it teaches | Required capability |
| --- | --- | --- |
| `minimal` | entry point and lifecycle | none |
| `extension` | define, query and call an `extension_get` function table | demo extension |
| `lz4` | compress and decompress a raw LZ4 block through the Host | LZ4 extension |
| `lvgl_ui` | core `host->graphics.lvgl` drawing, text and `on_loop` | none (core LVGL) |
| `framebuffer` | zero-copy lock/draw/unlock across Host framebuffer slices | display bitmap |
| `input` | button events and active exit | button |
| `imu` | gesture events and pull-based raw IMU | IMU events/raw |
| `bluetooth` | bidirectional channel + byte messages | Bluetooth |
| `device_state` | battery, charging, wearing and connection state | device state |
| `scene_bridge` | phone-driven text/rect/line/bitmap scene | display bitmap + Bluetooth |
| `web_bridge` | WebView-driven Scene rendering plus button/IMU uplink and framed LZ4 transfer | display bitmap + Bluetooth + button + IMU; optional LZ4 |
| `game/breakout` | complete local game | button + raw IMU + locale |
| `game/tetris` | grid game with IMU movement and button rotation | button + raw IMU + locale |
| `game/jet_runner` | scrolling IMU-controlled runner | button + raw IMU + locale |
| `game/snake` | IMU-controlled snake game | button + raw IMU + locale |
| `game/2048` | 4 x 4 number-merging puzzle with return-to-neutral input | button + raw IMU |
| `game/fighter_arena` | fixed two-fighter best-of-three match with specials and character AI | button + Bluetooth |

Build the changed module from the SDK root:

```sh
./gm-build
```

On Windows, use Windows PowerShell or Terminal (PowerShell) and run
`.\gm-build`. Do not use Command Prompt.

Build every maintained example:

```sh
./gm-build all
```

Audio is intentionally outside the plugin ABI. A plugin requests phone-side
playback/capture through Bluetooth messages; the glasses remain the phone's HFP
audio device. Persistent storage is also not currently exposed.
