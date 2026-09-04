# Plugin Open Platform

Plugin Open Platform is the public distribution for MemoMind plugin
development. It contains the public Web and glasses SDKs, examples,
documentation, and prebuilt MemoMind Plugin Studio applications.

The Desktop Studio source repository is not part of the public distribution.
Developers use the prebuilt applications under `Studio/`; they do not need the
Studio source to build, preview, package, or test plugins.

## Repository contents

- [`GlassSDK/`](GlassSDK/) contains the C SDK, build tools, examples, and
  documentation for glasses `.gmp` plugins.
- [`PhoneSDK/`](PhoneSDK/) contains the JavaScript SDK, examples, Browser Studio,
  DevKit ZIP tools, and `.mmpkg` packager for Web plugins.
- [`Studio/`](Studio/) contains prebuilt Desktop Studio applications and their
  platform runtime files.

## Desktop Studio platforms

The official desktop release targets are:

| Platform | Architecture | Distribution directory |
| --- | --- | --- |
| Windows 10/11 | x64 | `Studio/windows/` |
| Linux | x64 | `Studio/linux/` |
| macOS | Universal 2 (`x86_64` and Apple silicon) | `Studio/macos/` |

Traditional 32-bit x86 builds are not currently provided. Check the relevant
directory in your release checkout for the application and any required
runtime files. See [`Studio/README.md`](Studio/README.md) for installation,
startup, and platform requirements.

## Start developing

1. Download or clone this complete repository. Keep the `Studio`, `PhoneSDK`, and
   `GlassSDK` directories together so Desktop Studio can discover both SDKs.
2. Run the top-level build from this directory. It discovers Web and glasses
   plugins recursively and only rebuilds inputs that changed:

   ```sh
   # Ubuntu/macOS
   ./build.py

   # Windows PowerShell
   py build.py
   ```

   Add `--watch` to keep scanning, or append `web` or `glass` to select only
   one side, for example `./build.py --watch`, `./build.py web`, or
   `py build.py glass`.
   To display the complete command guide without compiling, use any one of
   `-h`, `--h`, `-help`, or `--help`:

   ```sh
   # Ubuntu/macOS
   ./build.py --help

   # Windows PowerShell
   py build.py --h
   ```

   All four help forms are equivalent. The help output includes commands for
   building everything, only PhoneSDK Web plugins, or only GlassSDK plugins,
   plus force, watch, output-directory, and platform examples.
3. Start the Desktop Studio application for your platform.
4. Follow the [Web Plugin quick start](PhoneSDK/docs/web-plugin/quick-start.md) or
   the [Glass Plugin SDK guide](GlassSDK/README.md).

The top-level cross-platform Python entry point delegates its Web portion to
`PhoneSDK/build.py`, which can also be run directly from inside PhoneSDK, while
CMake/Ninja incrementally builds RISC-V glasses plugins. Build state is local
to the top-level `.build/` for glasses and `PhoneSDK/.build/` for Web plugins;
distributable
`.mmpkg` and `.gmp` files remain in their SDK output directories. The generated
plugin packages can be consumed by the iOS App, but an iPhone or iPad is not a
build host. Any future native iOS target must run on macOS with Xcode; the same
Python entry point can dispatch that platform-only step.

Web-only development can also use the public, Node.js-based PhoneSDK Browser
Studio included in `PhoneSDK` and in the PhoneSDK DevKit ZIP. Browser Studio is a
lightweight Bridge and display simulator; Desktop Studio is the recommended
tool when a real `.gmp` must run together with a Web plugin.

## AI-assisted development

This repository includes an AI development skill at
`.agents/skills/memomind-plugin-dev/SKILL.md`. Codex automatically discovers
this repository-scoped skill when working inside the repository; no separate
Cloud-specific path is required.

If another AI agent does not support automatic skill discovery, ask it to read
`.agents/skills/memomind-plugin-dev/SKILL.md` explicitly before it builds,
runs, debugs, validates, or packages a plugin. Always refer to this relative
path instead of a machine- or Cloud-specific absolute path.

## Public support boundary

The SDK headers, JavaScript packages, examples, package formats, transport
specifications, and documents in this repository are the public development
contract. Desktop Studio is distributed as a prebuilt tool. Its private source
tree and internal build system are not required public dependencies.
