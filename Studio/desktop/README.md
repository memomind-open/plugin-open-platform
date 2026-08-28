# MemoMind Plugin Studio Desktop

The cross-platform desktop Studio runs a Web plugin and a device `.gmp` in one
window. The left side uses the system WebView; the right side uses the RV32/Host
simulation core from the adjacent `previewer/` directory.

## Current capabilities

- Tauri 2 provides one wrapper around WKWebView on macOS and WebView2 on
  Windows.
- An in-process localhost server with a random token serves the Web plugin, so
  ES modules, relative assets, and the plugin's own CSP behave consistently on
  both platforms. The server listens only on `127.0.0.1`.
- Open an `.mmpkg` directly or select a discovered Web plugin development
  directory. Studio reads the entry from `manifest.json` and falls back to
  `index.html` when no manifest exists.
- At startup, Studio scans `WebSDK/examples/*`, the single home for runnable
  Web plugin examples and complete workspaces. Output from tools such as Vite
  under `dist` or `build` is also supported. Studio starts in
  **Device only** mode and does not launch a Web plugin without an explicit
  selection.
- A device plugin is required; a Web plugin is optional. When developing a
  normal Web plugin without choosing a device plugin, Studio automatically
  selects and runs `web_bridge.gmp`. Selecting **Device only** unloads the Web
  view but retains the device display and all simulated single-click,
  double-click, long-press, continuous direction, and head-gesture controls.
- After a user chooses a device plugin, Studio preserves that selection when
  the Web plugin changes. This supports arbitrary paired plugins that exchange
  custom `plugin.sendMessage` channels.
- Studio reads `deviceRequirements` from the Web manifest and
  `provides.protocols` from the device manifest. It selects a device plugin by
  plugin ID, minimum version, and protocol versions, and marks choices as
  **Recommended**, **Compatible**, or **Incompatible**. An incompatible manual
  choice remains available for diagnosis but is not reported as ready.
- The Web side can import an `.mmpkg`. The device side scans built `.gmp` files
  under `GlassSDK/build-host` and can also import an individual `.gmp`.
- Selecting a Web plugin, or refreshing the repository, immediately runs the
  current selection and automatically rebuilds its `.mmpkg`. Studio serves the
  package through a LAN TCP service and keeps its installation QR code visible
  beside the selector. See
  [`../../WebSDK/docs/web-plugin/lan-install.md`](../../WebSDK/docs/web-plugin/lan-install.md)
  for the QR and download protocol.
- Selecting or refreshing a device plugin immediately loads and runs it, while
  Studio exposes its `.gmp` through the SDK-compatible `gmp+tcp` service and
  keeps the installation QR code visible beside the selector
  and display a QR code for Apps that support device plugin installation.
- Load and continuously run one device `.gmp`.
- Bridge v1 `plugin.sendMessage` calls
  `Previewer::sendBluetooth(channel, payload)` in the same process.
- Bridge v1 `display.*` calls are encoded with the Aphrodite Scene protocol for
  the default or a developer-selected device plugin. Text, GRAY_4, LZ4, and
  acknowledged atomic tiled frames are supported; routing is not hard-coded by
  plugin name.
- Tick the device plugin at 30 Hz and display its 600×350 GRAY_4 framebuffer on
  the right.
- Support runtime, storage, and connection queries plus button, `imuGesture`,
  `rawImu`, and connection events. Dragging the virtual head-motion stick drives
  proportional Raw IMU input; vertical movement gradually changes the
  persistent simulated head pitch. Nod and shake remain discrete firmware
  gesture events.
- Scene acknowledgements, standard device events, and custom channels sent by
  the device plugin through Host Bluetooth share one outbox. The first two are
  returned to the Bridge, while custom uplink messages remain visible in the
  Studio log.
- Safely extract `.mmpkg` files into temporary App storage while rejecting path
  traversal, symbolic links, excessive file counts, and decompression bombs.

One application therefore supports both of these pairings:

- `gm-life-desk.mmpkg + web_bridge.gmp`: full `display.*` and device event
  protocols.
- `fighter-controller.mmpkg + fighter_arena.gmp`: developer-defined
  `plugin.sendMessage` channels without the default `web_bridge.gmp`.

## Run locally

Install Rust, the platform C++ compiler, and the system WebView required by
Tauri. Cargo's `cc` build script compiles the C++ core directly; CMake is not
required.

```bash
cd Studio/desktop/src-tauri
cargo run
```

Before packaging, install the Tauri CLI and build the native installer on the
target platform:

```bash
cargo install tauri-cli --version 2.11.4 --locked
npm run build --prefix Studio
```

Automatic Web packaging uses the same `.mmpkg v1` structure and SHA-256 file
table as `WebSDK/tools/build-mmpkg.mjs`. Generated packages are written to the
SDK `dist/` directory. The QR payload contains the LAN host, TCP port, and
package name. The server reads the package at download time, so the same QR
continues to serve rebuilt content while those endpoint fields stay unchanged.
The phone and computer must be on the same LAN, and the App must implement the
`mmpkg+tcp` debugging installation protocol.

## Platform artifacts

Build and sign `.app/.dmg` artifacts on a macOS runner. Build and sign
`.msi/.exe` artifacts with MSVC on a Windows runner. Both platforms share all
Rust, frontend, and C++ core source. `Studio/previewer/apps/win32/main.cpp`
remains available as a standalone native Windows debugger.

For local Windows testing, Linux can cross-compile the unsigned x64 executable
with Tauri's `cargo-xwin` runner. Clang 19 or newer is required because the
desktop application compiles the bundled C++ previewer core against the MSVC
standard library:

```bash
rustup target add x86_64-pc-windows-msvc
cargo install --locked cargo-xwin
cargo install --locked tauri-cli --version 2.11.4
cd Studio/desktop/src-tauri
PATH=/usr/lib/llvm-19/bin:$PATH cargo tauri build \
  --runner cargo-xwin \
  --target x86_64-pc-windows-msvc \
  --no-bundle
```

The executable is written to
`Studio/.build/cargo/x86_64-pc-windows-msvc/release/gm-plugin-studio-desktop.exe`. Run it
from within a repository checkout so Studio can discover `WebSDK` and
`GlassSDK`. Production installers and code signing should still use a Windows
runner.

## Distribution exports

Native installers remain platform-specific. Use the common export workflow to
stage one directly usable file under `Studio/export/<platform>/`:

```bash
npm run export --prefix Studio
```

The Linux development environment can produce and stage an unsigned Windows
x64 NSIS installer with:

```bash
npm run export:windows:x64 --prefix Studio
```

See [`../export/README.md`](../export/README.md) for native installer guidance
and CI artifact collection.
