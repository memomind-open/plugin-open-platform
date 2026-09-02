# Troubleshooting

Read this file only after preserving the first actionable failure. Diagnose
the narrowest failing layer before changing dependencies or cleaning caches.

## SDK root or files not found

- Re-evaluate whether the developer provided the complete Plugin Open Platform
  release, a standalone GlassSDK, a WebSDK source tree, or an extracted DevKit.
- Check whether an archive is still unextracted.
- If multiple copies exist, compare manifests and versions and ask which copy
  is authoritative.
- Do not repair a missing Desktop Studio platform artifact by attempting to
  build the private Studio source.

## GlassSDK build failure

- Confirm the command ran from the GlassSDK root and used PowerShell on
  Windows.
- Confirm Python is 3.8 or newer and identify the interpreter actually used.
- Run the local build driver's help or `toolchain` command when selection is
  unclear.
- Distinguish network/download failure, checksum failure, compiler diagnostics,
  manifest validation, and packaging failure.
- If a custom `GM_RISCV_TOOLCHAIN` is set, verify it before unsetting or
  replacing it.
- Use `clean` only when evidence points to stale generated state.

## Web plugin does not start

- Verify Node.js 18 or newer and use the launcher from the detected local
  distribution.
- Confirm the selected directory is static and contains `manifest.json` at its
  root.
- For bundler projects, confirm the final `dist/` contains the vendored SDK
  module and uses package-relative imports.
- Inspect Browser Studio, Desktop Studio, and browser console logs for the first
  Bridge or CSP error.
- Do not run `npm ci` in an extracted DevKit; it is intended to run without
  third-party runtime dependencies.

## Studio does not start or discover SDKs

- Verify the platform and architecture are supported and that the platform
  directory contains a real release artifact.
- Restore the sibling `Studio/`, `WebSDK/`, and `GlassSDK/` layout if only the
  executable was moved.
- On Windows, verify `WebView2Loader.dll` remains beside the executable and
  check whether the Edge WebView2 Runtime is available.
- If a `.gmp` is absent from the selector, confirm it exists below
  `GlassSDK/build-host` or import the intended package explicitly.

## Input, display, or messages differ from expectations

- Confirm the correct plugin and compatible counterpart are selected.
- Verify focus is in the plugin input area before interpreting keyboard input
  as a plugin defect.
- Compare the event name, press/release state, locale, Bridge version, device
  profile, and payload size with the local documentation.
- Use Desktop Studio, not Browser Studio, for accessory input, raw head motion,
  real `.gmp` execution, or paired plugin messages.
- Reproduce on physical glasses before changing behavior that may be a
  simulator limitation.

## Packaging or device installation fails

- Confirm the packager input root contains `manifest.json` and all referenced
  files.
- Preserve the packager or App's exact validation error; do not bypass size,
  checksum, ABI, CSP, or memory checks.
- For QR installation, confirm the phone and development computer are on the
  same trusted LAN and that the selected host address is reachable.
- Avoid running two development servers on the same fixed port.

When reporting a blocker, include the failing command, exit status, first
actionable error, detected platform and versions, relevant artifact path, and
the smallest safe next step. Do not expose credentials or unrelated environment
data.
