---
name: memomind-plugin-dev
description: >-
  Build, run, debug, validate, and package MemoMind Web and glasses plugins
  from an already-downloaded SDK on Windows, macOS, or Linux. Use for local
  environment checks, plugin builds, Studio preview, packaging, and SDK
  troubleshooting. Do not use to download the SDK, modify the private Desktop
  Studio source, or publish code or releases.
---

# MemoMind Plugin Development

Work from the developer's already-downloaded and extracted SDK. Treat the SDK
headers, tools, manifests, and documentation in that local package as the
version-specific source of truth; do not silently replace them with newer
online instructions.

## Response language

- Follow the language explicitly requested by the developer.
- Otherwise, respond in the primary language of the developer's latest
  request: use Simplified Chinese for Chinese requests and English for English
  requests. Do not infer language from the operating system, region, account,
  or SDK path.
- Continue using the selected language throughout the current workflow unless
  the developer requests another language. If a request genuinely mixes both
  languages without a clear preference, use the language of the task's main
  instruction.
- Keep commands, command output, file names, paths, manifest fields, API and
  event identifiers, package names, version strings, and original error
  messages unchanged.
- When responding in Chinese, show the original error first, then explain its
  cause and next action in Simplified Chinese. Do not translate technical
  identifiers inside commands or source code.
- Preserve the product names MemoMind, GlassSDK, PhoneSDK, Desktop Studio, and
  Browser Studio in every response language.

## Establish the local context

1. Locate the relevant root without assuming a fixed absolute path:
   - A complete Plugin Open Platform release contains `GlassSDK/`, `PhoneSDK/`,
     `Studio/`, the short `tools/build` and `tools/build.cmd` launchers, and
     their shared top-level `build.py` implementation.
   - A GlassSDK root contains `build.py`, `include/`, and `examples/`.
   - A PhoneSDK source root contains `package.json`, `packages/`, `tools/`, and
     `examples/`.
   - An extracted PhoneSDK DevKit contains `DEVKIT-MANIFEST.json`, `sdk/`,
     `studio/`, `tools/`, and `examples/`.
2. If more than one candidate exists and the requested target is ambiguous,
   ask the developer which local package or plugin project to use.
3. Identify the host operating system and CPU architecture. Read
   [references/platforms.md](references/platforms.md) for supported platform
   combinations and platform-specific command rules.
4. Determine the requested development path. Read only the relevant guide:
   - Web plugin: [references/web-plugin.md](references/web-plugin.md)
   - Glasses `.gmp` plugin: [references/glass-plugin.md](references/glass-plugin.md)
   - Desktop or Browser Studio preview: [references/studio.md](references/studio.md)

## Operating rules

- Prefer the narrowest command that satisfies the request. Do not build every
  example when the developer asked for one plugin.
- Inspect local README files, manifests, package scripts, and tool help before
  inventing commands or file names.
- Work offline by default. A first GlassSDK build can download a pinned RISC-V
  toolchain plus CMake and Ninja; `npm ci` accesses the configured npm
  registry. Explain this before running either operation when the required
  cache or dependencies are absent.
- Do not install system software, modify system environment variables, change
  global tool versions, use credentials, or alter security settings without
  explicit authorization.
- Do not clone, pull, switch branches, commit, push, publish, or upload unless
  the developer separately requests that action.
- Do not modify Desktop Studio binaries or assume its private source is part of
  the public SDK.
- Preserve the complete `Studio/`, `PhoneSDK/`, and `GlassSDK/` sibling layout
  when using Desktop Studio; moving only the executable prevents automatic SDK
  discovery.
- Use local relative paths in plugin source and packages. Never embed a
  developer's machine-specific absolute path in distributable output.
- Treat simulator results as development evidence, not final hardware proof.

## Execute and verify

From a complete Plugin Open Platform root, prefer `./tools/build` on
macOS/Linux or `.\tools\build` in Windows PowerShell to discover and
incrementally build both SDKs. Append `web` or `glass` when the developer
requests only one side, and append `--watch` only when continuous rebuilding
is requested. The launchers share the top-level `build.py`; continue to use
each SDK's narrower command when the developer asks for one specific plugin.

Before changing a plugin, inspect its manifest and the closest maintained
example. Preserve the local SDK's ABI, Bridge, lifecycle, packaging, and
capability rules.

After each requested operation, verify an observable result instead of relying
only on command exit status:

- Build: the expected `.gmp`, `.mmpkg`, or static output exists and has a
  current timestamp.
- Preview: the appropriate Studio starts, loads the requested plugin, and
  exposes useful runtime logs.
- Integration: required button, accessory, gesture, locale, display, and
  Web-to-glasses message paths behave as applicable.
- Packaging: `manifest.json` is at the package root and the local packager
  accepts the output.
- Device preparation: Desktop Studio selects the intended package and exposes
  its developer-app QR code on the intended trusted LAN.

If an operation fails, preserve the first actionable error and read
[references/troubleshooting.md](references/troubleshooting.md). Avoid broad
cleanups or unrelated dependency upgrades until the failure is understood.

## Report the outcome

Conclude with:

- detected SDK layout, platform, architecture, and local SDK version when
  available;
- actions performed and checks that passed;
- absolute paths to produced artifacts and relevant logs;
- simulator or hardware validation still required;
- blockers and the smallest next action when completion was not possible.
