# MemoMind Plugin Studio

MemoMind Plugin Studio is the default cross-platform simulator for this monorepo. It
runs a companion-app Web plugin on the left and a selectable device `.gmp`
plugin on the right in the same Tauri application.

```sh
npm run dev
```

The default command uses Cargo's optimized Release profile for smoother
simulation. Run `npm run dev:debug` when a Debug build is needed.

The `desktop/` directory contains the Tauri application. The `previewer/`
directory contains the reusable RV32 interpreter, GMP loader, simulated Host
API, standalone CLI and optional Win32 compatibility UI.

The `scripts/` directory owns all workspace-level build, test, container, and
export automation.
Generated, independently distributable packages are staged under `export/` by
platform and are not committed. See [`export/README.md`](export/README.md).

The desktop application compiles the previewer core directly through Cargo and
does not require CMake. See `desktop/README.md` and `previewer/README.md` for
detailed platform notes.

Build the internal Windows x64 executable without creating an installer:

```sh
npm run build:windows:x64 --prefix Studio
```

This command always overwrites
`.build/cargo/x86_64-pc-windows-msvc/release/gm-plugin-studio-desktop.exe`.

## Source layout

```text
Studio/
├── desktop/
│   ├── ui/
│   │   ├── src/       Browser runtime modules and styles
│   │   └── tests/     Browser runtime regression tests
│   └── src-tauri/     Native application and Tauri configuration
├── previewer/
│   ├── core/          Reusable RV32 and simulated device implementation
│   ├── include/       Public C and C++ previewer interfaces
│   ├── apps/          Standalone CLI and Win32 compatibility frontends
│   ├── resources/     Native frontend resources
│   ├── tests/         Core integration and SDK regression tests
│   └── third_party/   Pinned LVGL compatibility dependency
├── scripts/           Build, test, container, and export orchestration
├── .build/            Ignored native build output
└── export/            Ignored platform-specific distribution output
```
