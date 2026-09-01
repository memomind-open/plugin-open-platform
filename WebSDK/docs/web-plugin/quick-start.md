# Quick Start

The public WebSDK is available directly in Plugin Open Platform and as
`gm-web-plugin-devkit-<version>.zip`. The DevKit ZIP includes the browser SDK,
WebSDK Browser Studio, packager, examples, and documentation, so it does not
require access to an npm registry.

The complete Plugin Open Platform release also contains a prebuilt Desktop
Studio under `Studio/<platform>/`. Desktop Studio is recommended when a Web
plugin must run together with a glasses `.gmp`.

## 1. Add the local SDK

Copy the standalone SDK file from the DevKit ZIP into your plugin project:

```sh
mkdir -p ./vendor
cp /path/to/gm-web-plugin-devkit/sdk/gm-plugin-web-sdk.esm.js ./vendor/
```

When working from the full WebSDK source tree, run `npm run build:devkit` first
and use the `sdk/` directory from the generated ZIP.

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

The SDK automatically detects the App WebView or a supported Studio Host, so
plugin business logic does not need separate implementations.

## 2. Debug the plugin

### Recommended: Desktop Studio

Start the prebuilt application in `plugin-open-platform/Studio/<platform>/`,
then select the plugin workspace or built directory from the Web plugin
selector. Select a compatible glasses plugin when the test requires a real
`.gmp`, device rendering, or custom plugin messages.

Keep `Studio`, `WebSDK`, and `GlassSDK` together in the repository layout so
Desktop Studio can discover the public examples automatically.

### Lightweight option: WebSDK Browser Studio

For a static H5 project with `index.html` at its root:

```sh
node /path/to/WebSDK/tools/run-browser-studio.mjs \
  --plugin /absolute/path/to/my-plugin
```

The DevKit ZIP provides the same launcher at a shorter path:

```sh
node /path/to/gm-web-plugin-devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin
```

For Vite, Webpack, or a similar tool, build first and load the final output:

```sh
npm run build

node /path/to/gm-web-plugin-devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin/dist
```

Browser Studio starts at `http://127.0.0.1:4173` by default. Add `--port 4174`
to use another port. It does not execute `.gmp` files.

## 3. Build an App plugin package

Make sure the final build directory contains `manifest.json` at its root, then
build the `.mmpkg`:

```sh
node /path/to/WebSDK/tools/build-mmpkg.mjs \
  /absolute/path/to/my-plugin/dist \
  /absolute/path/to/release/my-plugin-1.0.0.mmpkg
```

The DevKit ZIP contains the same tool under `tools/build-mmpkg.mjs`. See
[Final `.mmpkg` package](package-format.md) for the complete format and
security limits.

## Future WebSDK package workflow

If the WebSDK libraries are published to npm in the future, plugin projects
may replace the vendored SDK file with the matching public package. Desktop
Studio remains a prebuilt Plugin Open Platform application, and Browser Studio
remains part of the public WebSDK and DevKit ZIP; no public Desktop Studio
source or npm package is required.
