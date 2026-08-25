# Quick Start

The current preview release is distributed as
`gm-web-plugin-devkit-<version>.zip`. It includes the browser SDK, Studio,
packager, examples, and this documentation, so it does not require access to an
npm registry.

## 1. Add the local SDK

Copy the SDK file from the DevKit into your plugin project:

```sh
mkdir -p ./vendor
cp /path/to/gm-web-plugin-devkit/sdk/gm-plugin-web-sdk.esm.js ./vendor/
```

Import it with a relative path:

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();
await gm.ready();

await gm.display.updateText({
  id: 1,
  x: 20,
  y: 20,
  width: 300,
  height: 60,
  border: 1,
  radius: 8,
  text: 'Hello GM',
});
```

The SDK automatically detects a real App WebView or the simulated Studio Host,
so plugin business logic does not need separate implementations.

## 2. Debug with Studio

For a static H5 project with `index.html` at its root:

```sh
node /path/to/gm-web-plugin-devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin
```

For a project built with Vite, Webpack, or a similar tool, build it first and
load the final output directory:

```sh
npm run build

node /path/to/gm-web-plugin-devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin/dist
```

The default URL is `http://127.0.0.1:4173`. Add `--port 4174` to use another
port.

## 3. Build an App plugin package

Make sure the build directory contains `manifest.json` at its root, then build
the `.mmpkg`:

```sh
node /path/to/gm-web-plugin-devkit/tools/build-mmpkg.mjs \
  /absolute/path/to/my-plugin/dist \
  /absolute/path/to/release/my-plugin-1.0.0.mmpkg
```

See [Final `.mmpkg` package](package-format.md) for the complete format and
security limits.

## Future npm workflow

The ZIP is a temporary distribution method for the preview phase. After the
npm packages are published, install them with:

```sh
npm install @memomind/gm-plugin-web-sdk
npm install --save-dev @memomind/gm-plugin-studio
```

```js
import { createGMPlugin } from '@memomind/gm-plugin-web-sdk';
```

Start Studio through the package command:

```sh
npx gm-plugin-studio --plugin ./dist
```

Migrating from the ZIP to npm does not change the Bridge API, manifest, or
`.mmpkg` format.
