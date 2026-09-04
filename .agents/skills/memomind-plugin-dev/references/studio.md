# Studio Selection and Validation

Plugin Open Platform provides two different simulators. Choose based on the
requested capability, not convenience alone.

## Desktop Studio

Use the prebuilt Desktop Studio when the workflow needs any of the following:

- execution of a glasses `.gmp`;
- paired Web and glasses plugin message routing;
- accessory navigation buttons or proportional raw head motion;
- separate Web and glasses simulator logs;
- combined locale, lifecycle, display, and input testing.

Keep this layout intact:

```text
plugin-open-platform/
|-- GlassSDK/
|-- PhoneSDK/
`-- Studio/<platform>/
```

Verify that the platform directory contains a real application, not only a
placeholder. On Windows, start the application from the release root with:

```powershell
.\Studio\windows\gm-plugin-studio-desktop.exe
```

Keep `WebView2Loader.dll` beside the executable. If the application reports a
missing WebView runtime, explain the requirement before proposing installation.
For macOS or Linux, inspect the included package or executable and the local
`Studio/README.md`; do not guess a release-specific launch command.

Select the requested Web workspace, static build, or `.mmpkg` and the requested
built or imported `.gmp`. Confirm that selection focus, input events, locale,
display output, message routing, and both log panes work as applicable.

## PhoneSDK Browser Studio

Use Browser Studio for lightweight Web-only Bridge, lifecycle, event, and
rendering tests. It requires Node.js 18 or newer and a modern browser. It does
not execute `.gmp` files and cannot prove paired Web/glasses integration.

Use the launcher from the detected distribution:

- Full PhoneSDK: `node tools/run-browser-studio.mjs --plugin <absolute-path>`
- DevKit: `node studio/gm-plugin-studio.mjs --plugin <absolute-path>`

The default URL is `http://127.0.0.1:4173`. Use an alternate documented port
when it is occupied. Point the launcher at a static directory; build Vite,
Webpack, or similar projects first.

## Validation boundary

Both simulators are development tools. Do not claim they prove physical
brightness, optics, sensor thresholds and noise, firmware scheduling,
Bluetooth timing, or target memory pressure. Explicitly list remaining
physical-device checks in the outcome report.
