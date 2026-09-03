# Glasses Plugin Workflow

Use this guide for native RV32 glasses plugins packaged as `.gmp` files.

## Prepare the local SDK

Confirm the GlassSDK root contains `build.py`, `include/`, and `examples/`.
Verify Python 3.8 or newer. A system C compiler, GNU Make, CMake, and Ninja are
not prerequisites: the SDK driver manages its pinned build components.

The first build can download and verify the pinned RISC-V toolchain and install
the CMake and Ninja Python packages. Explain this before the first run when the
cache is absent. If the developer supplies `GM_RISCV_TOOLCHAIN`, verify that it
contains a compatible `riscv-none-elf-gcc` rather than replacing it.

## Create or change a plugin

1. Read the local `GlassSDK/README.md`, the relevant example README, and the
   public headers under `include/`.
2. Start from the closest maintained example. Use `examples/minimal` for the
   smallest lifecycle reference and `examples/game/breakout` for a complete UI
   and input example.
3. Give a new example directory its own `manifest.json` and at least one `.c`
   file.
4. Do not add an example-local `CMakeLists.txt`; the SDK-root build discovers
   example manifests and C sources.
5. Keep entry, load, start, stop, and unload responsibilities consistent with
   the local ABI documentation. Use only validated Host and extension tables.

## Build narrowly

Run commands from the GlassSDK root. Use `py build.py` on Windows PowerShell and
`python3 build.py` on macOS or Linux.

Build one top-level example:

```text
python3 build.py build --example bluetooth
```

Build one nested example:

```text
python3 build.py build --example game/2048
```

Use `python3 build.py all` only when the developer requests a complete
maintained example build. Use `python3 build.py inspect --example <path>` when
ELF inspection is needed. On Windows, replace `python3` with `py`. Do not run
`clean` as a generic first fix because it removes outputs and the current
platform's build cache.

Generated artifacts are under `build-host/.build/<example>/`. Locate and report
the actual `.gmp` rather than guessing its file name.

## Preview or install

- For local simulation, preserve the complete public repository layout and use
  a compatible prebuilt Desktop Studio. Read [studio.md](studio.md).
- For physical glasses, read the local `INSTALLATION.md`, select the intended
  package in Desktop Studio, and use Studio's developer-app QR code.
- The Studio QR channel is unauthenticated and unencrypted. Use it only on a
  trusted LAN.
- Only one plugin is currently installed at a time, and it is not persisted
  across a glasses reboot.

## Verification

- Confirm the `.gmp` exists below `build-host/.build/` and that the build
  selected the expected RISC-V toolchain.
- Preview lifecycle, display, button/accessory input, gesture or raw IMU,
  locale, Bluetooth, and plugin logs as applicable.
- For a physical installation, confirm the App accepts the package and the
  plugin starts on the glasses.
- Treat ABI conformance and simulator behavior as necessary but not sufficient;
  validate sensor noise, scheduling, Bluetooth timing, memory pressure, and
  optical output on target hardware.
