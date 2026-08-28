# MemoMind Plugin Studio exports

This directory is the local staging area for independently distributable
MemoMind Plugin Studio builds. Generated delivery files are intentionally
ignored by Git.

Native desktop applications require platform-specific packages, so exports are
grouped by operating system and CPU architecture:

```text
export/
├── windows-x64/
├── windows-arm64/
├── linux-x64/
├── linux-arm64/
├── macos-x64/
└── macos-arm64/
```

The standard export commands produce exactly one directly usable delivery file
per platform:

- Windows: an NSIS `.exe` installer
- Linux: a portable `.AppImage`
- macOS: a `.dmg` disk image when built on macOS

Build and export for the current host platform:

```sh
npm run export --prefix Studio
```

Build one Ubuntu-compatible Linux x64 AppImage in the pinned Ubuntu 24.04
container:

```sh
npm run export:linux:x64 --prefix Studio
```

This keeps the build independent from an older host's GLib and WebKitGTK
versions. Docker is required, and the current user must be able to access its
daemon.

Cross-compile one unsigned Windows x64 NSIS installer from the configured Linux
development environment:

```sh
npm run export:windows:x64 --prefix Studio
```

The export script automatically detects `.windows-build-tools/bin` beside the
workspace or under the current home directory. Set `GM_STUDIO_TOOL_DIR` when
`cargo-tauri` and `cargo-xwin` are installed elsewhere.

To collect artifacts produced by CI or another build system without rebuilding,
the lower-level collection command can still retain multiple files plus a
manifest and checksums:

```sh
npm run export:collect --prefix Studio -- \
  --platform windows-x64 \
  --input /path/to/MemoMind-Plugin-Studio.msi
```

Release Windows installers should still be signed on Windows, macOS packages
on macOS, and Linux packages on the oldest supported Linux distribution. The
generated Windows installer can bootstrap Microsoft WebView2.
