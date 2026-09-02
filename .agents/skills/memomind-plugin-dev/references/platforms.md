# Platform Selection

Read this file after locating the local SDK and before choosing commands.

## Supported combinations

| Component | Windows | macOS | Linux |
| --- | --- | --- | --- |
| GlassSDK build tools | x64 | Intel and Apple silicon | x64 and ARM64 |
| Desktop Studio | Windows 10/11 x64 | Universal 2 | x64 |
| WebSDK Browser Studio | Node.js 18+ and a modern browser | Node.js 18+ and a modern browser | Node.js 18+ and a modern browser |

Traditional 32-bit x86 Desktop Studio builds are not distributed. A source
checkout may contain only a placeholder for some Studio platforms, so verify
that the expected application artifact exists before proposing Desktop Studio.

## Environment checks

Check only tools required for the selected path:

- GlassSDK: Python 3.8 or newer. The SDK driver provides or installs its pinned
  build components and does not require a system C compiler, GNU Make, CMake,
  or Ninja.
- WebSDK DevKit: Node.js 18 or newer and a modern browser. Its Browser Studio
  and packager have no third-party runtime dependencies.
- WebSDK source workspace: Node.js 18 or newer and npm. Use the lockfile with
  `npm ci` when dependencies must be installed.
- Windows Desktop Studio: keep `WebView2Loader.dll` beside the executable; the
  Microsoft Edge WebView2 Runtime may also be required.

Report detected versions before recommending installation. Do not install
every platform prerequisite preemptively.

## Command conventions

### Windows

- Run GlassSDK commands in PowerShell or Windows Terminal using PowerShell.
  Command Prompt is not supported.
- From the GlassSDK root, invoke the driver as `./gm-build.exe` or
  `.\gm-build`. Prefer the spelling documented by the local SDK checkout.
- Quote paths with spaces and use resolved absolute paths when passing a plugin
  directory to Studio or packaging tools.

### macOS and Linux

- Invoke the GlassSDK driver as `./gm-build` from the SDK root.
- If executable permission was lost while copying or extracting the SDK, use
  `chmod +x gm-build`, then retry. Do not change permissions recursively.
- Inspect the actual files under `Studio/macos/` or `Studio/linux/` before
  choosing a launch command; release packaging can vary.

## Architecture mismatch

If Desktop Studio is unavailable for the detected architecture, do not attempt
to rebuild its private source. Web-only work can use Browser Studio. GlassSDK
plugins can still be built on supported GlassSDK hosts and validated on
physical glasses; explain that local `.gmp` simulation requires a compatible
prebuilt Desktop Studio artifact.
