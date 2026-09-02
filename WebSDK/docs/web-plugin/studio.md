# Debugging with Desktop Studio and Browser Studio

Plugin Open Platform provides two public debugging tools. They share Bridge v1
semantics but cover different development stages.

## Choose the right tool

| Capability | Desktop Studio | WebSDK Browser Studio |
| --- | --- | --- |
| Distribution | Prebuilt application under `Studio/<platform>/` | Public WebSDK source and DevKit ZIP |
| Web plugin loading | Workspace or `.mmpkg` | Static build directory |
| Glasses plugin loading | Executes `.gmp` through the RV32 previewer | Not supported |
| Web-to-glasses plugin messages | Routed to the running `.gmp` | Validated and logged without a real `.gmp` |
| Virtual 600 x 350 display | Yes | Yes |
| Primary and accessory buttons | Yes | Primary button only |
| Raw head-motion input | Yes | Discrete gesture injection |
| Runtime logs | Web and glasses simulator logs | Bridge Inspector |
| Native desktop dependencies | Platform WebView and bundled runtime files | Node.js 18 and a modern browser |

Use Desktop Studio for normal development and every paired Web/glasses plugin
test. Use Browser Studio for quick H5, Bridge, rendering, lifecycle, and CI
checks that do not require a `.gmp`.

## Desktop Studio

Download the complete Plugin Open Platform release and keep this layout:

```text
plugin-open-platform/
|-- GlassSDK/
|-- WebSDK/
`-- Studio/<platform>/
```

Start the application from the platform directory, then use the single
**Import workspace** action in the top toolbar to select the
`plugin-open-platform` root. Studio discovers valid plugin manifests one, two,
or three plugin levels below a first-level collection directory in `WebSDK` and
`GlassSDK`, excluding SDK infrastructure directories. The `examples`
collections remain recommended but are not required. For a glasses plugin, Studio loads a GMP beside
the manifest or from the corresponding relative directory below
`GlassSDK/build-host/.build`. Deeper source directories are not scanned, but
their built `.mmpkg` and `.gmp` packages can be imported directly.

Desktop Studio provides:

- automatic compatible Web and glasses plugin pairing;
- primary button, accessory navigation, and proportional head-motion input;
- Web-to-glasses and glasses-to-Web plugin messages;
- separate Web and glasses simulator logs;
- interface language selection and glasses plugin locale propagation;
- `.mmpkg` and `.gmp` development QR generation; and
- GRAY_4 framebuffer and text-overlay preview.

The Desktop Studio application is distributed as a prebuilt tool. Its private
source repository is not a public SDK dependency.

## WebSDK Browser Studio

From the public `WebSDK` directory, run:

```sh
npm ci
node tools/run-browser-studio.mjs --plugin examples/app-counter
```

For a Vite, Webpack, or similar project, build it first and point Browser
Studio at the final static output:

```sh
npm run build
node /path/to/WebSDK/tools/run-browser-studio.mjs \
  --plugin /absolute/path/to/my-plugin/dist
```

The default address is `http://127.0.0.1:4173`. Use `--port 4174` when the
default port is unavailable. The same launcher is included in the DevKit ZIP
as `studio/gm-plugin-studio.mjs`.

Browser Studio can inject primary-button actions, discrete head gestures,
connection changes, and lifecycle states. It displays Bridge requests and
events and renders supported text, GRAY_4, LZ4, and atomic tile operations.

## Transport validation

Both tools enforce the Scene Bridge payload limit of 81,901 bytes. Oversized
compressed or decoded Channel 6, Channel 7, or Channel 9 image payloads fail
with `PAYLOAD_TOO_LARGE`. Split large images into independently compressed
tiles and validate the result again on physical glasses.

Neither simulator is an optical or performance guarantee. Test brightness,
projection, timing, Bluetooth, sensor behavior, firmware integration, and
memory pressure on the target device before release.
