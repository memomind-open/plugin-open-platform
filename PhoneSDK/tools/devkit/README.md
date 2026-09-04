# GM Web Plugin DevKit 0.1.0 (Preview Draft)

This archive provides an early GM Web Plugin development workflow before the
npm packages are published. It contains:

- `sdk/`: the browser ES module SDK and TypeScript declarations used by the App
  and Web plugins.
- `studio/`: the PhoneSDK Browser Studio launcher for Web-only simulation.
- `tools/`: the final `.mmpkg` packager.
- `examples/`: the runnable App Counter example.
- `docs/web-plugin/`: draft public developer documentation.

Requirement: Node.js 18 or later.

Browser Studio simulates the App Bridge and virtual display in a normal
browser. It does not load or execute glasses `.gmp` files. Use the prebuilt
Desktop Studio from Plugin Open Platform when testing paired Web and glasses
plugins.

## Run immediately

```sh
node studio/gm-plugin-studio.mjs --plugin examples/app-counter
```

Open `http://127.0.0.1:4173`.

## Use the SDK in your plugin

Copy `sdk/gm-plugin-web-sdk.esm.js` into the final plugin output, for example
under `vendor/`:

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();
await gm.ready();
```

## Build an App-importable package

```sh
node tools/build-mmpkg.mjs \
  /absolute/path/to/plugin/dist \
  /absolute/path/to/release/plugin-1.0.0.mmpkg
```

Start with `docs/web-plugin/README.md` for the full workflow. See
`docs/web-plugin/devkit-zip.md` for ZIP-specific details. These paths refer to
the generated DevKit ZIP layout.

## Distribution model

This DevKit ZIP remains the offline distribution for the standalone browser
SDK, Browser Studio, packager, examples, and documentation. Desktop Studio is
distributed separately as a prebuilt application inside Plugin Open Platform;
its source and an npm Studio package are not required.
