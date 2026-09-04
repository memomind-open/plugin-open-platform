# DevKit ZIP Guide

The preview ZIP has this structure:

```text
gm-phone-plugin-devkit-0.1.0/
├── README.md
├── DEVKIT-MANIFEST.json
├── package.json
├── sdk/
│   ├── gm-plugin-web-sdk.esm.js
│   └── gm-plugin-web-sdk.d.ts
├── studio/
│   └── gm-plugin-studio.mjs
├── tools/
│   └── build-mmpkg.mjs
├── examples/
│   └── counter/
├── docs/web-plugin/
└── internal/
```

The directories relevant to plugin developers are:

- `sdk/`: copy these files into the H5 project and import them by relative path.
- `studio/`: contains the public PhoneSDK Browser Studio launcher. It locally
  simulates the App Bridge, device display, primary button, and lifecycle, but
  does not execute glasses `.gmp` files.
- `tools/`: packages the final H5 build directory as an `.mmpkg`.
- `examples/`: runnable examples that already reference the local SDK.
- `docs/web-plugin/`: the current draft developer documentation.
- `internal/`: Browser Studio runtime dependencies that developers do not need
  to modify or import directly.

## Requirements

- Node.js 18 or later.
- A modern browser such as Chrome, Edge, or Safari.
- Browser Studio and the packager have no third-party runtime dependencies.

## Five-minute walkthrough

Extract the archive and enter its directory:

```sh
unzip gm-phone-plugin-devkit-0.1.0.zip
cd gm-phone-plugin-devkit-0.1.0
```

Run the included example:

```sh
node studio/gm-plugin-studio.mjs --plugin examples/app-counter
```

Open `http://127.0.0.1:4173` in PhoneSDK Browser Studio, then:

1. Select **Draw to glasses** and confirm that a green device image appears on
   the right.
2. Use Browser Studio's single-click, double-click, and head-gesture buttons
   and confirm that the plugin receives the events.
3. Change the device connection and lifecycle state and confirm that the plugin
   handles the changes.
4. Inspect requests, responses, and events in the Bridge Inspector.

Package the included example:

```sh
node tools/build-mmpkg.mjs \
  examples/app-counter \
  release/counter-0.1.0.mmpkg
```

## Integrate an existing H5 project

Copy the SDK file into a directory that will be included in the final build:

```sh
mkdir -p /path/to/my-plugin/public/vendor
cp sdk/gm-plugin-web-sdk.esm.js \
  /path/to/my-plugin/public/vendor/
```

Build tools handle static directories differently. Confirm that
`dist/vendor/gm-plugin-web-sdk.esm.js` exists in the final output and that
business code uses a package-relative URL. Never reference an absolute path on
the computer containing the DevKit from the final plugin.

Typical debug and packaging commands are:

```sh
npm run build

node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin /path/to/my-plugin/dist

node /path/to/devkit/tools/build-mmpkg.mjs \
  /path/to/my-plugin/dist \
  /path/to/my-plugin/release/my-plugin-1.0.0.mmpkg
```

## Current preview limitations

- Browser Studio loads a static directory; it does not start Vite, provide HMR,
  or execute `.gmp` files. Use the prebuilt Desktop Studio from Plugin Open
  Platform for paired Web and glasses plugin debugging.
- The browser SDK is a single-file ES module, with separate type declarations
  in `sdk/`.
- The current App supports unsigned `.mmpkg` sideloading only in Debug builds.
- Browser Studio fonts, brightness, and optical effects require final
  validation on physical glasses.
- The DevKit ZIP remains an official offline distribution for the browser SDK,
  Browser Studio, packager, examples, and documentation.
