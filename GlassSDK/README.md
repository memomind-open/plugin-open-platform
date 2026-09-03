# GM Plugin SDK

GM Plugin SDK lets third-party developers build applications independently from
the glasses firmware. A plugin is compiled as RV32 position-independent code
and packed into a `.gmp` file. It accesses glasses features only through the
versioned Host API; it does not link firmware symbols or private driver data.

The SDK is self-contained and supports Windows x64, macOS Intel/Apple Silicon,
and Linux x64/ARM64. Python 3.8 or newer is required. GNU Make, CMake, and a
system-wide compiler installation are not required.

When GlassSDK is kept inside the complete Plugin Open Platform directory, run
`../tools/build glass` (`..\tools\build glass` in Windows PowerShell) to use
the top-level discovery and incremental-build entry point. The GlassSDK-local
commands below remain available for standalone use and single-plugin builds.

## Quick start

Open a terminal in the SDK root. The first command builds every plugin. Later
commands use CMake and Ninja's compiler dependency graph to rebuild only the
affected source files and packages.

Windows (PowerShell only):

1. Press `Win+X`, then open **Terminal (PowerShell)** or **Windows PowerShell**.
2. Change to the SDK root and run:

```powershell
py build.py
```

macOS or Linux:

```sh
python3 build.py
```

The output is `build-host/.build/<changed-example>/<example>.gmp`.
The internal CMake cache is kept separately under
`build-host/.build/<platform>-<architecture>` on macOS/Linux and under
`%LOCALAPPDATA%\GMPluginSDK\cmake` on Windows. This lets a shared SDK checkout
build from Windows, macOS, and Linux without cache or Windows UNC conflicts.

The first build downloads the pinned xPack RISC-V GCC 15.2.0-1 toolchain to the
current user's system cache and verifies its SHA-256. Later builds and other
copies of this SDK reuse the cached compiler. Set
`GM_RISCV_TOOLCHAIN` to a directory containing `riscv-none-elf-gcc` to use an
existing compatible toolchain instead.

The driver also installs CMake and Ninja through the Python interpreter it
starts when they are absent. Third-party developers do not need to install GNU
Make, a system compiler, CMake, or Ninja first.

## Build your first plugin

Start with [`examples/game/breakout`](examples/game/breakout). It demonstrates
a complete plugin lifecycle, LVGL UI, button input, raw IMU control, locale
handling, scoring, and game state.

After changing its source or manifest, run `python3 build.py` from the SDK root.

On Windows, use PowerShell only: press `Win+X`, open **Terminal (PowerShell)**,
then run `py build.py`.
Copy the example directory when starting a new plugin, then update its
`manifest.json`, source, and callback implementation. Keep `gm_plugin_entry`
limited to Host validation and descriptor setup; acquire resources in
`on_load`, create UI in `on_start`, and release them in the matching teardown
callbacks.

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

Do not add an example-local `CMakeLists.txt`. The internal build definition
discovers this directory and every `.c` file below it automatically. Run the
same root command after editing; on the first run it builds all examples, and
later runs rebuild only the compiler-detected dependencies.

The prebuilt Desktop Studio does not require source changes or a known plugin
name. Studio recursively discovers `examples/<relative-path>/manifest.json`
and loads its package from the matching
`build-host/.build/<relative-path>/` directory after a build. Use Studio's
refresh action to display the new plugin automatically.

## Build commands

| Command | Description |
| --- | --- |
| `python3 build.py` | Rebuild all affected plugins |
| `python3 build.py build --example bluetooth` | Build one example |
| `python3 build.py build --example game/2048` | Build one nested example |
| `python3 build.py all` | Build all maintained examples |
| `python3 build.py inspect --example extension` | Build and inspect the RISC-V ELF |
| `python3 build.py clean` | Remove temporary build files while keeping prebuilt GMP packages |
| `python3 build.py toolchain` | Display the selected compiler |

Use `py build.py` on Windows PowerShell and `python3 build.py` on macOS/Linux. Outputs are
stored under `build-host/.build/<example>/`.

## Preview and delivery

The build command only creates `.gmp` packages and exits. Open MemoMind Plugin
Studio, refresh the workspace, and select the generated package for simulation.
Studio owns developer-app ZIP assembly, sharing, and QR generation for the
current phone/glasses selection, including glasses-only plugins.

Examples do not need their own `CMakeLists.txt`. The build driver discovers
every example directory containing `manifest.json` and compiles all of its
`.c` files, whatever their names.

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

See [ABI.md](docs/ABI.md) for the complete ABI and lifecycle contract and
[GRAPHICS.md](docs/GRAPHICS.md) for LVGL and framebuffer rules.

## Install on glasses

After building, use the prebuilt Desktop Studio under `../Studio/<platform>/`
for local simulation. To install on physical glasses, select the package in
Studio and scan Studio's developer-app QR code from the official App's
Developer Workbench entry.

The public workflow does not require a separate installer source repository.
See [INSTALLATION.md](docs/INSTALLATION.md) for the complete preview and
build-to-glasses workflow and [PROTOCOL.md](docs/PROTOCOL.md) for the
underlying transport contract.

## Examples

Maintained examples cover individual Host modules, product integration, and
complete games including Breakout, Tetris, Jet Runner, Snake, 2048, and Fighter Arena. See
[examples/README.md](examples/README.md) for the complete list.

Build every maintained example:

```sh
python3 build.py all
```

## Documentation

This root `README.md` is the public entry point. Detailed specifications and
guides live under `docs/`.

The top-level layout intentionally exposes only the files most developers
need:

| Path | Purpose |
| --- | --- |
| `build.py` | Single cross-platform build and inspection entry point |
| `examples/` | Plugin source examples to copy or modify |
| `include/` | Public SDK headers |
| `docs/` | ABI, graphics, installation, protocol, and security guides |
| `build-host/` | Build output under `.build/` and internal scripts under `tools/` |

CMake files, linker inputs, and dependency lists are kept under
`build-host/tools/`; GMP packages and disposable platform-specific build files
are kept under `build-host/.build/`. Repository CI
configuration, internal test suites, and source-art production files are not
included in the published SDK.

| Document | Purpose |
| --- | --- |
| [INSTALLATION.md](docs/INSTALLATION.md) | Install, start, stop, replace, and remove `.gmp` packages |
| [ABI.md](docs/ABI.md) | ABI compatibility, lifecycle, callbacks, memory, and Host services |
| [GRAPHICS.md](docs/GRAPHICS.md) | Shared LVGL and direct framebuffer rendering |
| [SECURITY.md](docs/SECURITY.md) | Package integrity, trust, and isolation limits |
| [PROTOCOL.md](docs/PROTOCOL.md) | Phone/PC-to-glasses transport protocol |
| [PROTOCOL_COMPATIBILITY.md](docs/PROTOCOL_COMPATIBILITY.md) | Declare Web/device protocol requirements and compatibility |
| [CAPABILITY_MATRIX.md](docs/CAPABILITY_MATRIX.md) | Mapping from applications to low-level Host services |
| [examples/README.md](examples/README.md) | Maintained examples and build instructions |

The public headers under `include/` are the canonical API definition.

## Frequently asked questions

### Where is the compiler installed?

The downloaded toolchain is cached under `%LOCALAPPDATA%\GMPluginSDK\toolchains`
on Windows, `~/Library/Caches/GMPluginSDK/toolchains` on macOS, and
`$XDG_CACHE_HOME/GMPluginSDK/toolchains` or `~/.cache/GMPluginSDK/toolchains`
on Linux. Run `py build.py toolchain` on Windows or
`python3 build.py toolchain` on macOS/Linux to display the selected compiler.

### What is the difference between `.elf` and `.gmp`?

The ELF file under `build-host/.build/<platform>-<architecture>/` contains
developer and relocation metadata used by `build.py inspect`. The glasses and
Desktop Studio load the compact `.gmp` package from `build-host/.build/`, not
the complete ELF file.

### Why is the plugin gone after reboot?

Plugins currently run from RAM and are not persisted in Flash. Start Desktop
Studio again and reinstall the `.gmp` with Studio's QR code through the official
App's Developer Workbench after the glasses reboot.

### Which graphics API should I use?

Use LVGL for text, controls, and ordinary UI. Use direct framebuffer access for
games, decoders, or specialized pixel rendering. Do not nest LVGL calls inside
a framebuffer lock. See [GRAPHICS.md](docs/GRAPHICS.md).

### Can an untrusted `.gmp` be installed safely?

No. The current function table is an ABI boundary, not a security sandbox. Only
install packages from trusted sources; see [SECURITY.md](docs/SECURITY.md).
