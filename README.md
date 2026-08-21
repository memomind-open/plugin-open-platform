# GM Plugin Open Platform

This monorepo contains the public development tools for both GM glasses device
plugins and companion-app Web plugins.

## Repository layout

```text
GMPluginSDK/       Native RV32 `.gmp` SDK, examples, packaging and install tools
GMWebPluginSDK/    Bridge v1 Web SDK, browser simulator, `.mmpkg` tools and docs
GMPluginStudio/    Cross-platform dual-ended Studio and the GMP previewer core
```

The two package formats serve different runtimes: `.gmp` runs on the glasses,
while `.mmpkg` runs in the companion application's WebView. GM Plugin Studio
loads one of each so their standard Scene transport or custom message channels
can be tested together.

## Quick start

Run the cross-platform desktop Studio:

```sh
npm run dev
```

This starts the optimized Release build by default. For Rust debugging, use
`npm run dev:debug` instead.

Run repository-level JavaScript tests:

```sh
npm test
```

Build all native device examples from `GMPluginSDK/`:

```sh
sh ./gm-build all
```

Each component has its own README with platform prerequisites and focused
commands.
