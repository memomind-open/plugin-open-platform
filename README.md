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
- [`WebSDK/`](WebSDK/) contains the JavaScript SDK, examples, Browser Studio,
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

1. Download or clone this complete repository. Keep the `Studio`, `WebSDK`, and
   `GlassSDK` directories together so Desktop Studio can discover both SDKs.
2. Start the Desktop Studio application for your platform.
3. Follow the [Web Plugin quick start](WebSDK/docs/web-plugin/quick-start.md) or
   the [Glass Plugin SDK guide](GlassSDK/README.md).

Web-only development can also use the public, Node.js-based WebSDK Browser
Studio included in `WebSDK` and in the WebSDK DevKit ZIP. Browser Studio is a
lightweight Bridge and display simulator; Desktop Studio is the recommended
tool when a real `.gmp` must run together with a Web plugin.

## Public support boundary

The SDK headers, JavaScript packages, examples, package formats, transport
specifications, and documents in this repository are the public development
contract. Desktop Studio is distributed as a prebuilt tool. Its private source
tree and internal build system are not required public dependencies.
