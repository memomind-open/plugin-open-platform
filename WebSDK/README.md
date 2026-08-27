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

The last command serves `examples/basic-counter` in the browser-only Studio at
`http://127.0.0.1:4173`.

Build a distributable Web plugin package:

```sh
npm run pack:plugin -- examples/basic-counter dist/basic-counter.mmpkg
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
examples/                 All runnable Web plugin examples and workspaces
tools/browser-studio      Browser-only compatibility Studio UI
tools/                    Development servers, packagers and repository checks
docs/web-plugin/          Web plugin developer documentation
dist/                     Generated packages and DevKit archives (ignored)
node_modules/             Installed dependencies (ignored)
```

The `examples/` directory is the single home for Counter, Momo Talking Pet,
Fighter Arena Controller, GM Life Desk, Tic-Tac-Toe, and Weather. Static
examples run directly in the desktop Studio; TypeScript/Vite examples must be
built before their generated `dist/` output appears in the Studio selector.
Generated outputs and dependency directories are not stored in the repository.

## Directory boundaries

- Put reusable SDK code in `packages/<package-name>/` and keep package tests
  beside that package.
- Put every runnable plugin, whether static or toolchain-based, in
  `examples/<plugin-name>/`.
- Put repository automation and compatibility Studio implementation in
  `tools/`.
- Put public Web plugin documentation in `docs/web-plugin/`.
- Never place generated packages, dependencies, or local build output under a
  source directory unless the tool requires an ignored `dist/` directory.
