# MemoMind Plugin Studio

This directory contains the prebuilt MemoMind Plugin Studio applications
distributed with Plugin Open Platform. Desktop Studio runs a Web plugin and a
glasses `.gmp` plugin in one window, provides a 600 x 350 virtual display, and
simulates the public Host and Bridge APIs used during plugin development.

Desktop Studio is published as a binary tool. Its source repository is
maintained privately and is not required to use the public SDKs.

## Supported releases

| Directory | Release target | Notes |
| --- | --- | --- |
| `windows/` | Windows 10/11 x64 | Keep the EXE and accompanying runtime DLLs together |
| `linux/` | Linux x64 | Use the package or portable executable included in the release |
| `macos/` | macOS Universal 2 | Supports Intel and Apple silicon Macs |

Traditional 32-bit x86 builds are not currently distributed.

Not every source checkout is a complete release bundle. If a platform
directory contains only a placeholder, obtain a release that includes the
platform artifact before following the startup instructions.

## Start Desktop Studio

Keep the repository layout intact:

```text
plugin-open-platform/
|-- GlassSDK/
|-- WebSDK/
`-- Studio/
    |-- windows/
    |-- linux/
    `-- macos/
```

Use the single **Import workspace** button in the top toolbar to select the
`plugin-open-platform` directory. Studio then scans valid plugin manifests one,
two, or three plugin levels below each SDK's first-level collection directories
(for example, `WebSDK/examples/<plugin>/manifest.json`). SDK infrastructure
such as tools, dependencies, documentation, and build output is excluded. The two
plugin cards retain only their refresh actions. Use the separate **Import
package** button beside **Import workspace** for `.mmpkg`, `.gmp`, and complete
developer-app `.zip` files; unrelated file types are hidden.
Studio can also locate the SDKs automatically when the repository layout
remains intact.

Studio does not contain a built-in list of example or plugin names. Add a
plugin at `GlassSDK/examples/<relative-path>/`, run `GlassSDK/build.py`, and
refresh Studio: it discovers the example's `manifest.json` recursively, then
loads the generated package from the matching
`GlassSDK/build-host/.build/<relative-path>/` output directory.

`WebSDK/examples` and `GlassSDK/examples` remain the recommended locations, but
they are not required. A plugin can live one to three levels below any
non-infrastructure first-level collection directory. Deeper source workspaces
are not scanned; import their built package through the top toolbar instead.
Importing a ZIP applies its complete A/B composition, so a component absent
from the ZIP is disabled in Studio just as it is during App local import.

For glasses plugins, manifest metadata is used only for display and optional
pairing hints. Unknown fields are ignored, and unavailable metadata does not
block execution. Studio validates an imported GMP package before running or
sharing it; the physical glasses validates it again when loading it.

On Windows, run:

```powershell
.\Studio\windows\gm-plugin-studio-desktop.exe
```

Keep `WebView2Loader.dll` beside the Windows executable. Windows may also
require the Microsoft Edge WebView2 Runtime. On Linux and macOS, open the
application package included in the matching platform directory.

## What Desktop Studio provides

- Unified platform-workspace discovery and top-level `.mmpkg`, `.gmp`, or
  developer-app ZIP import.
- Glasses `.gmp` loading through the software RV32 and Host API previewer.
- Web-to-glasses plugin message routing.
- Virtual display, primary button, accessory navigation buttons, and raw head
  motion input.
- Runtime logs for both the Web plugin and glasses simulator.
- Interface language selection and plugin locale propagation.
- One developer-app ZIP and QR for the selected phone/glasses combination.
  New installs fetch the complete selection; updates transfer only changed
  components. See [`APP_BUNDLE.md`](../APP_BUNDLE.md).

Simulation is not a substitute for final testing on physical glasses. Optical
brightness, timing, Bluetooth behavior, sensors, memory pressure, and firmware
integration must still be validated on the target device.

## WebSDK Browser Studio

The separate WebSDK Browser Studio is public source under
`WebSDK/tools/browser-studio` and is also included in the WebSDK DevKit ZIP. It
runs through Node.js and a normal browser and is useful for quick Web-only
Bridge, lifecycle, event, and rendering tests.

Browser Studio does not execute `.gmp` files or simulate the complete glasses
plugin runtime. Use Desktop Studio when testing a Web plugin together with a
glasses plugin. See the
[Web Plugin Studio guide](../WebSDK/docs/web-plugin/studio.md) for a detailed
comparison.
