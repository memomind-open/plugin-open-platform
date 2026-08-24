# Repository operating rules

This is a public monorepo. Public documentation, source comments, change
descriptions, and Git metadata must be understandable without access to
internal company systems.

## Default simulator

- The default interactive application is the cross-platform dual-ended Studio
  under `Studio/desktop`.
- When a request only says to run, start, or open the simulator, run `npm run
  dev` from the repository root.
- The browser-only Studio and standalone Win32 previewer are compatibility
  tools and must be started explicitly.

## Pairing behavior

- Treat both sides as selectable inputs: a Web `.mmpkg` or development
  directory and any compatible device `.gmp`.
- Keep both standard `display.*` Scene transport and generic
  `plugin.sendMessage` channel transport working.

## Changes

- Use English Conventional Commit messages and English pull request content.
- Do not push directly to `main`.
- Do not commit generated build directories, downloaded toolchains, packaged
  plugins, dependency directories, or platform installers.
