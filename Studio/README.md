# GM Plugin Studio

GM Plugin Studio is the default cross-platform simulator for this monorepo. It
runs a companion-app Web plugin on the left and a selectable device `.gmp`
plugin on the right in the same Tauri application.

```sh
npm run dev
```

The default command uses Cargo's optimized Release profile for smoother
simulation. Run `npm run dev:debug` when a Debug build is needed.

The `desktop/` directory contains the Tauri application. The `previewer/`
directory contains the reusable RV32 interpreter, GMP loader, simulated Host
API, standalone CLI and optional Win32 compatibility UI.

The desktop application compiles the previewer core directly through Cargo and
does not require CMake. See `desktop/README.md` and `previewer/README.md` for
detailed platform notes.
