# GM Web Plugin DevKit 0.1.0 (Preview Draft)

This archive provides an early GM Web Plugin development workflow before the
npm packages are published. It contains:

- `sdk/`: the browser ES module SDK and TypeScript declarations used by the App
  and Web plugins.
- `studio/`: the local GM Plugin Studio launcher.
- `tools/`: the final `.mmpkg` packager.
- `examples/`: the Counter example configured to use the local SDK.
- `docs/web-plugin/`: draft public developer documentation.

Requirement: Node.js 18 or later.

## Run immediately

```sh
node studio/gm-plugin-studio.mjs --plugin examples/basic-counter
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

Start with [docs/web-plugin/README.md](docs/web-plugin/README.md) for the full
workflow. See [docs/web-plugin/devkit-zip.md](docs/web-plugin/devkit-zip.md) for
ZIP-specific details.

## Future npm migration

After official publication, the planned workflow is:

```sh
npm install @memomind/gm-plugin-web-sdk
npm install --save-dev @memomind/gm-plugin-studio
npx gm-plugin-studio --plugin ./dist
```

Migrating to npm does not change the Bridge API, manifest, or `.mmpkg` format.
