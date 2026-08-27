# GM Web Plugin SDK

GM Web Plugin SDK is the development kit for Web plugins that run in a
companion application and render content on GM glasses through Bridge v1.

This workspace contains:

- the browser SDK used by plugin authors;
- the shared Bridge contract;
- a simulated application runtime;
- a GRAY_4 device renderer;
- the browser-only compatibility Studio;
- deterministic `.mmpkg` and DevKit packaging tools.

The default dual-ended desktop application lives in the sibling `Studio/`
workspace. This directory remains independently testable and
can produce a standalone Web developer kit.

## Quick start

```sh
npm ci
npm test
npm run dev
```

The last command serves `examples/counter` in the browser-only Studio at
`http://127.0.0.1:4173`.

Build a distributable Web plugin package:

```sh
npm run pack:plugin -- examples/counter dist/counter.mmpkg
```

Build the standalone developer kit ZIP:

```sh
npm run build:devkit
```

## Layout

```text
packages/bridge-contract  Public methods, events, errors and device profile
packages/web-sdk          H5 SDK and App/Studio transports
packages/device-renderer  GRAY_4 framebuffer and Canvas presentation
packages/studio-runtime   Simulated App host
browser-studio            Browser-only compatibility Studio UI
examples/counter          Minimal Web plugin
examples/talking-pet      Interactive talking-pet game
plugins                   Complete maintained Web plugin workspaces
tools                     Studio server, packagers and repository checks
docs/web-plugin           Web plugin developer documentation
```

The `plugins/` directory contains Counter, Fighter Arena Controller, GM Life
Desk, Tic-Tac-Toe, and Weather. Static plugins run directly in the desktop
Studio; TypeScript/Vite plugins must be built before their generated `dist/`
output appears in the Studio selector. Generated outputs and dependency
directories are not stored in the repository.
