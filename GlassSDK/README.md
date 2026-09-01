# GM Plugin SDK

GM Plugin SDK lets third-party developers build applications independently from
the glasses firmware. A plugin is compiled as RV32 position-independent code
and packed into a `.gmp` file. It accesses glasses features only through the
versioned Host API; it does not link firmware symbols or private driver data.

The SDK is self-contained and supports Windows x64, macOS Intel/Apple Silicon,
and Linux x64/ARM64. Python 3.8 or newer is required. GNU Make, CMake, and a
system-wide compiler installation are not required.

## Quick start

Open a terminal in the SDK root. The first command builds every plugin. Later
commands use CMake and Ninja's compiler dependency graph to rebuild only the
affected source files and packages, then start a local QR installation server.

Windows (PowerShell only):

1. Press `Win+X`, then open **Terminal (PowerShell)** or **Windows PowerShell**.
2. Change to the SDK root and run:

```powershell
.\gm-build
```

macOS or Linux:

```sh
chmod +x gm-build   # only if executable permission was lost after copying
./gm-build
```

The output is `build-host/<changed-example>/<example>.gmp`.
The internal CMake cache is kept separately under
`build-host/.cmake/<platform>-<architecture>` on macOS/Linux and under
`%LOCALAPPDATA%\GMPluginSDK\cmake` on Windows. This lets a shared SDK checkout
build from Windows, macOS, and Linux without cache or Windows UNC conflicts.

The first build downloads the pinned xPack RISC-V GCC 15.2.0-1 toolchain and
verifies its SHA-256. Later builds use the cached copy. Set
`GM_RISCV_TOOLCHAIN` to a directory containing `riscv-none-elf-gcc` to use an
existing compatible toolchain instead.

The driver also installs CMake, Ninja, and the QR generator through the Python
interpreter it starts when they are absent. Third-party developers do not need
to install GNU Make, a system compiler, CMake, Ninja, or the QR package first.

## Build your first plugin

Start with [`examples/game/breakout`](examples/game/breakout). It demonstrates
a complete plugin lifecycle, LVGL UI, button input, raw IMU control, locale
handling, scoring, and game state.

After changing its source or manifest, run `./gm-build` from the SDK root.

On Windows, use PowerShell only: press `Win+X`, open **Terminal (PowerShell)**,
then run `.\gm-build`.
Copy the example directory when starting a new plugin, then update its
`manifest.json`, source, and callback implementation. Keep `gm_plugin_entry`
limited to Host validation and descriptor setup; acquire resources in
`on_load`, create UI in `on_start`, and release them in the matching teardown
callbacks.

For the smallest possible lifecycle reference without UI, see
[`examples/minimal`](examples/minimal).

### Create an example

Copy an existing example directory below `examples/`, then change its
`manifest.json` identity and source files. An example needs a `manifest.json`
and at least one `.c` file; source file names are descriptive by convention,
not fixed. For example:

```text
examples/hello/
  manifest.json
  hello.c
```

Do not add an example-local `CMakeLists.txt`. The SDK-root build definition
discovers this directory and every `.c` file below it automatically. Run the
same root command after editing; on the first run it builds all examples, and
later runs rebuild only the compiler-detected dependencies.

## Build commands

| Command | Description |
| --- | --- |
| `gm-build` | Rebuild affected plugins, then serve the most recently updated GMP |
| `gm-build build --example bluetooth` | Build one example |
| `gm-build build --example game/2048` | Build a nested game example |
| `gm-build all` | Build all maintained examples |
| `gm-build inspect --example minimal` | Build and inspect the RISC-V ELF |
| `gm-build clean` | Remove outputs and the current platform's CMake cache; the next build is full |
| `gm-build toolchain` | Display the selected compiler |

Use `.\gm-build` on Windows PowerShell and `./gm-build` on macOS/Linux. Outputs are
stored under `build-host/<example>/`.

## QR package selection

After the incremental build completes, the driver serves the valid example
`.gmp` with the newest modification time. This also applies to the first full
build, a no-change run, and a shared-input change that rebuilds several
packages. If modification times are identical, the example path provides a
stable tie-breaker.

Examples do not need their own `CMakeLists.txt`. The SDK-root
[`CMakeLists.txt`](CMakeLists.txt) discovers every example directory containing
`manifest.json` and compiles all of its `.c` files, whatever their names.

## Core concepts

- A `.gmp` contains native RV32 plugin code for the glasses. It is not a
  Windows, macOS, Linux, or Android application.
- The firmware owns the Host API, LVGL runtime, framebuffer, and hardware
  services. Plugins call only validated Host function tables.
- Lifecycle callbacks run serially on the display task and must remain short
  and non-blocking.
- `on_load` manages load-lifetime resources. `on_start` creates UI and starts a
  visible application cycle. `on_stop` and `on_unload` release the corresponding
  resources.
- Optional functionality is discovered through independently versioned
  extension tables.
- Only one plugin is currently installed at a time, and plugins are not
  persisted across a glasses reboot.

See [ABI.md](ABI.md) for the complete ABI and lifecycle contract and
[GRAPHICS.md](GRAPHICS.md) for LVGL and framebuffer rules.

## Install on glasses

After building, use the prebuilt Desktop Studio under `../Studio/<platform>/`
for local simulation. To install on physical glasses, run the SDK QR server and
scan its `gmp+tcp` code from the official App's GMP debug installation entry.

The public workflow does not require a separate installer source repository.
See [INSTALLATION.md](INSTALLATION.md) for the complete preview and
build-to-glasses workflow and [PROTOCOL.md](PROTOCOL.md) for the underlying
transport contract.

## Examples

Maintained examples cover individual Host modules, product integration, and
complete games including Breakout, Tetris, Jet Runner, Snake, 2048, and Fighter Arena. See
[examples/README.md](examples/README.md) for the complete list.

Build every maintained example:

```sh
./gm-build all
```

## Documentation

| Document | Purpose |
| --- | --- |
| [INSTALLATION.md](INSTALLATION.md) | Install, start, stop, replace, and remove `.gmp` packages |
| [ABI.md](ABI.md) | ABI compatibility, lifecycle, callbacks, memory, and Host services |
| [GRAPHICS.md](GRAPHICS.md) | Shared LVGL and direct framebuffer rendering |
| [SECURITY.md](SECURITY.md) | Package integrity, trust, and isolation limits |
| [PROTOCOL.md](PROTOCOL.md) | Phone/PC-to-glasses transport protocol |
| [PROTOCOL_COMPATIBILITY.md](PROTOCOL_COMPATIBILITY.md) | Declare Web/device protocol requirements and compatibility |
| [CAPABILITY_MATRIX.md](CAPABILITY_MATRIX.md) | Mapping from applications to low-level Host services |
| [examples/README.md](examples/README.md) | Maintained examples and build instructions |

The public headers under `include/` are the canonical API definition.

## Contributing

Use GitHub issues to report public bugs or propose enhancements. Pull requests
are welcome; changes are reviewed and validated before they are published to
the public repository.

## Frequently asked questions

### Where is the compiler installed?

The downloaded toolchain is cached under
`%LOCALAPPDATA%\GMPluginSDK\toolchains` on Windows and `.toolchains/` inside the
SDK on macOS/Linux. Run `gm-build toolchain` to display the selected compiler.

### What is the difference between `.elf` and `.gmp`?

The ELF file contains developer and relocation metadata used for inspection.
The glasses load the compact `.gmp` package, not the complete ELF file.

### Why is the plugin gone after reboot?

Plugins currently run from RAM and are not persisted in Flash. Start the SDK or
Desktop Studio QR server again and reinstall the `.gmp` through the official
App's GMP debug installation entry after the glasses reboot.

### Which graphics API should I use?

Use LVGL for text, controls, and ordinary UI. Use direct framebuffer access for
games, decoders, or specialized pixel rendering. Do not nest LVGL calls inside
a framebuffer lock. See [GRAPHICS.md](GRAPHICS.md).

### Can an untrusted `.gmp` be installed safely?

No. The current function table is an ABI boundary, not a security sandbox. Only
install packages from trusted sources; see [SECURITY.md](SECURITY.md).

## Serve a plugin to Aphrodite over the local network

Build the changed example and start its local TCP file server. The first run
installs its QR generator automatically when needed. Use the command that
matches the host platform.

macOS or Linux (shell paths use `/`):

```sh
./gm-build
```

Windows (PowerShell only):

1. Press `Win+X`, then open **Terminal (PowerShell)** or **Windows PowerShell**.
2. Change to the SDK root and run:

```powershell
.\gm-build
```

The default command uses CMake and Ninja, not Git. Ninja reads the compiler's
dependency graph: a changed `.c` rebuilds only that object and GMP; a changed
header, manifest, or linker input rebuilds every affected GMP. The first run
after `clean` builds every example. CMake, Ninja, the RISC-V compiler, and the
QR dependency are installed automatically when absent. With no changed input,
it serves the most recently updated valid example package already present in
`build-host`.

Command Prompt is not supported. On Windows, `gm-build.exe` is a native
PowerShell launcher, so it is not subject to the PowerShell script-signing or
batch-file execution policies. `Ctrl+C` is passed directly to the QR server,
which closes its socket and exits without a batch confirmation.
Both `/` and `\` example separators are normalized internally, but the examples
above intentionally use the native spelling for each platform.

After compilation finishes, `serve` prints a QR code directly in the terminal
and writes the same code to a `*.qr.png` image next to the generated GMP.
macOS and Linux use compact half-block characters. Windows uses a wider
ANSI-background form because some Windows terminal fonts render half-block
characters as hollow boxes, which makes the code unscannable. Use the PNG when
a smaller Windows display is preferable.
The command keeps serving that file until you press `Ctrl+C`. Open **Settings > Device
information > Debug > Plugin WebView Demo > Scan GMP QR code** in Aphrodite,
scan the code, and keep the phone and development computer on the same LAN. The
QR code contains a compact `gmp+tcp://host:port/package.gmp` URI with only the
stable server endpoint and package name. You can
rebuild the same example in another terminal and scan the original QR code
again to fetch the new GMP; the server calculates its size and SHA-256 for every
download. This is an unauthenticated Debug LAN channel: the size, SHA-256, and
GMP CRC detect truncation or inconsistent content, but do not authenticate the
server or resist an active attacker who can replace both data and checksum. Use
`--host <address>` when automatic network-interface selection
does not choose the address reachable by the phone. Port `18765` is used by
default so the QR code also stays stable across server restarts; use `--port 0`
when a random free port is preferable. An already-built package can be served
with `--gmp <path>`.
