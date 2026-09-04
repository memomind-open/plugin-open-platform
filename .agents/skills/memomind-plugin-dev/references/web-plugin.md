# Web Plugin Workflow

Use this guide for Web plugins that produce an `.mmpkg` and run in the App
WebView. First distinguish the full PhoneSDK source workspace from the extracted
offline DevKit.

## Choose the local distribution

### Extracted PhoneSDK DevKit

Confirm that the root contains `DEVKIT-MANIFEST.json`, `sdk/`, `studio/`,
`tools/`, and `examples/`. It is intended to work without npm registry access.

- Local SDK module: `sdk/gm-plugin-web-sdk.esm.js`
- Type declarations: `sdk/gm-plugin-web-sdk.d.ts`
- Browser Studio launcher: `studio/gm-plugin-studio.mjs`
- Packager: `tools/build-mmpkg.mjs`

Copy the SDK module into a plugin-owned directory that is included in the
final static build, such as `vendor/`. Import it by a package-relative path.
Never reference the absolute DevKit location from distributable code.

### Full PhoneSDK source workspace

Confirm that `package.json` identifies the GM Web Plugin SDK workspace. Read
its current scripts before running them.

- Install locked development dependencies only when required: `npm ci`.
- Run the workspace's documented validation, normally `npm test` or
  `npm run verify`.
- Use `tools/run-browser-studio.mjs` for Web-only debugging.
- Use `tools/build-mmpkg.mjs` or the documented package script to create an
  `.mmpkg`.
- Build the DevKit ZIP only when explicitly requested.

Do not run `npm ci` merely to use an already-extracted DevKit.

## Create or adapt a plugin

1. Inspect a maintained local example and the local Web plugin quick start.
2. Ensure the plugin initializes the SDK and waits for readiness according to
   the installed SDK version.
3. Keep `manifest.json` at the root of the static directory passed to Studio
   and the packager.
4. For Vite, Webpack, or another bundler, build first and select the final
   static output such as `dist/`, not the source directory.
5. Keep Bridge methods, lifecycle handling, device profiles, payload limits,
   and plugin message protocols consistent with the local documentation.

## Debug and package

From an extracted DevKit, start Web-only debugging with:

```sh
node studio/gm-plugin-studio.mjs --plugin /absolute/path/to/plugin-or-dist
```

From a full PhoneSDK workspace, use:

```sh
node tools/run-browser-studio.mjs --plugin /absolute/path/to/plugin-or-dist
```

Package a final static directory with the packager from the applicable local
distribution:

```sh
node tools/build-mmpkg.mjs /absolute/path/to/dist /absolute/path/to/release/plugin-version.mmpkg
```

Use Desktop Studio instead when the test requires accessory navigation, raw
head motion, a real `.gmp`, or Web-to-glasses plugin message routing.

## Verification

- Confirm the plugin reaches its ready state without Bridge connection errors.
- Exercise the events and lifecycle states used by the plugin.
- Inspect requests, responses, and events in the available logs or Bridge
  Inspector.
- Verify visible text or GRAY_4 output on the virtual display when applicable.
- Confirm the `.mmpkg` exists and was produced from a directory whose root
  contains `manifest.json`.
- Perform final optical, Bluetooth, timing, firmware, and physical-device
  validation outside the simulator.

For API, manifest, CSP, payload, and release details, use the documentation in
the local DevKit or `PhoneSDK/docs/web-plugin/`; do not duplicate or override it.
