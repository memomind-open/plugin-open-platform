# Fighter Arena Windows simulator

This simulator compiles the production `fighter_arena/fighter_arena.c` directly into
a small Win32 Host. It is intended for fast local iteration on combat, input,
AI, collision timing, and GRAY_4 rendering. It is not an RV32 emulator and does
not validate GMP packaging, firmware scheduling, Bluetooth transport, or the
optical appearance of the glasses.

From the SDK root, build and run it with:

```powershell
.\gm-sim
```

Run the headless lifecycle and framebuffer smoke test with:

```powershell
.\gm-sim --smoke
```

Run deterministic combat scenarios and save checkpoint screenshots with:

```powershell
.\gm-sim --test-combat
```

The report and BMP checkpoints are written under
`build-host/simulator/test-results`.

Run the complete hidden lifecycle scenario with:

```powershell
.\gm-sim --test-lifecycle
```

Lifecycle screenshots and the report are written under
`build-host/simulator/lifecycle-results`.

The game controls are `A`, `D`, `W`, `S`, `J`, `K`, `L`, `I`, and `Enter`.
Simulator controls are:

- `Esc`: pause or resume
- `F5`: restart the plugin
- `F6`: toggle the debug overlay
- `F7` / `F8`: decrease or increase simulation speed
- `F10`: advance one 50 ms game frame while paused
- `F12`: save a lossless window screenshot

Screenshots are written under `build-host/simulator/screenshots`. Sound events
are logged to stderr; the simulator does not duplicate the Windows installer's
WAV assets.
