---
name: memomind-plugin-delivery
description: Build, package, and check MemoMind Web/Glass plugins, review attachments, DevKit bundles, and public SDK distribution artifacts. Use for .mmpkg/.gmp or Studio delivery preparation; it does not authorize commits, publication, or private Desktop Studio builds.
---

# Build and delivery

First identify the deliverable: a Web `.mmpkg`, Glass `.gmp`, DevKit ZIP, or combined application ZIP exported by Studio. Check sources, manifests, versions, and the requested scope. Document paths are relative to this file; working directories for build commands are stated below.

For new applications, see [running and verification](../memomind-example-app/references/run-and-verify.md) for source directories, dist output, preview arguments, and common errors. PhoneSDK `build.py` has no single-project filter. Prefer building the new project directly and invoking the packager to avoid rewriting unrelated distribution packages.

## Build entry points

- Repository root: run `python3 build.py --help` for options. Select a platform with `python3 build.py web` or `python3 build.py glass`; use a full build only when the delivery needs it.
- `GlassSDK/`: build one plugin with `python3 build.py build --example <relative-path>`. Use `--project` for an independent project; see the [GlassSDK README](../../../GlassSDK/README.md). The first build may download a pinned toolchain; do not replace its version or checksum to bypass failures.
- `PhoneSDK/`: use `python3 build.py --help` for discovery, build, and output behavior. For a single H5 project, run its own build first, then package it with `npm run pack:plugin -- /absolute/plugin/dist /absolute/release/plugin.mmpkg`. The output must be outside the input directory. For static examples, use their actual deployable directory as input.

## Packages and review attachments

Web packaging rules are in [package format](../../../PhoneSDK/docs/web-plugin/package-format.md). The manifest, entry point, permissions, and file hashes must match the final output. Do not manually edit archives, hashes, or signatures to hide validation failures.

Glass builds produce `.gmp`, `.review.json`, and `.review-source.enc`; keep the matching set from the same build. Read [REVIEW_PACKAGES](../../../GlassSDK/docs/REVIEW_PACKAGES.md): Studio uses existing attachments rather than collecting the current working tree again. Rebuild after changing sources or dependencies. External dependencies must satisfy the SDK/toolchain/source-root collection boundaries rather than silently disappearing from the captured inputs.

Public distribution encrypts review sources with a public key. Do not copy private keys or internal reviewer tools into the public repository. Encryption, CRCs, hashes, or successful rebuilds do not establish review approval, trusted signing, or a native memory sandbox.

For DevKit, read [devkit ZIP](../../../PhoneSDK/docs/web-plugin/devkit-zip.md) and run `npm run build:devkit` from `PhoneSDK/`. Inspect actual ZIP contents and referenced files without incidentally changing release versions or other packages.

## Preview and delivery evidence

Read the [Studio README](../../../Studio/README.md), [INSTALLATION](../../../GlassSDK/docs/INSTALLATION.md), and [LAN install](../../../PhoneSDK/docs/web-plugin/lan-install.md). Desktop Studio imports GMP, MMPKG, and combined ZIP packages; Browser Studio simulates only the Web Bridge. Preserve the SDK/Studio layout and plugin discovery depth conventions.

Builds write to some tracked distribution directories. Compare git status before and after building and check the packages and attachments generated for this task. Preserve existing user changes and avoid incidental replacement of Studio executables/installers. When a task requires private Desktop Studio source, state the missing dependency and complete the independent work that is possible.

Run targeted tests appropriate to the task. Public PhoneSDK changes use `npm run verify`; Glass packaging/ABI changes use relevant `tests/`. For skill-documentation-only edits, validate skill structure, document links, and the diff without rebuilding the entire SDK.

Report package paths, corresponding source revision/working-tree state, checks actually run, simulator/device results, and unverified items. Do not report publication without deployment or hardware success without physical-device verification. Determine signing, online review, and marketplace support from current documentation and implementation rather than treating plans as shipped capabilities.
