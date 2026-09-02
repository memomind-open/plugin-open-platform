# GM Web Plugin SDK

GM Web Plugin SDK is the public development kit for Web plugins that run in a
companion App WebView and communicate with GM glasses through Bridge v1.

This workspace contains:

- the browser SDK used by plugin authors;
- the shared Bridge contract;
- a simulated application runtime;
- a GRAY_4 device renderer;
- the public, Node.js-based WebSDK Browser Studio;
- deterministic `.mmpkg` and DevKit ZIP packaging tools; and
- runnable examples and public documentation.

The recommended full debugger is the prebuilt MemoMind Desktop Studio under
[`../Studio/`](../Studio/). Desktop Studio can run a Web plugin together with a
real glasses `.gmp` in the software previewer. Its source repository is not
part of the public SDK distribution.

WebSDK Browser Studio remains available for lightweight Web-only development.
It simulates the App Bridge and virtual display in a normal browser, but it does
not load or execute `.gmp` files.

## Quick start

Install the WebSDK development dependencies and run its validation:

```sh
npm ci
npm test
```

Start Browser Studio with the Counter example:

```sh
npm run dev
```

Open `http://127.0.0.1:4173`. For paired Web and glasses plugin development,
start the prebuilt Desktop Studio for your platform and select the Web workspace
and glasses plugin there.

Build a distributable Web plugin package:

```sh
npm run pack:plugin -- examples/app-counter dist/app-counter.mmpkg
```

Build the standalone DevKit ZIP, which includes Browser Studio:

```sh
npm run build:devkit
```

## Layout

```text
packages/bridge-contract  Public methods, events, errors, and device profile
packages/web-sdk          H5 SDK and App/Studio transports
packages/device-renderer  GRAY_4 framebuffer and Canvas presentation
packages/studio-runtime   Browser Studio Host simulation
examples/                 Runnable Web plugin examples and workspaces
tools/browser-studio      Public Web-only compatibility Studio UI
tools/                    Development servers, packagers, and checks
docs/web-plugin/          Public Web plugin documentation
dist/                     Published or locally generated packages and DevKit ZIPs
```

Static examples can run directly in Desktop Studio or Browser Studio.
TypeScript/Vite examples must be built before their generated `dist/` output is
selected. Official Plugin Open Platform releases may include generated
`.mmpkg` and DevKit artifacts under `WebSDK/dist`; local builds may overwrite
or add files there.

## Directory boundaries

- Put reusable SDK code in `packages/<package-name>/` and keep package tests
  beside that package.
- Put every runnable plugin, whether static or toolchain-based, somewhere below
  `examples/`; nested organization such as `examples/vendor/<plugin-name>/` is
  supported.
- Keep Browser Studio implementation under `tools/browser-studio` and its
  reusable Host implementation under `packages/studio-runtime`.
- Put public Web plugin documentation in `docs/web-plugin/`.
- Do not depend on the private Desktop Studio source repository from WebSDK
  packages, examples, or public documentation.
