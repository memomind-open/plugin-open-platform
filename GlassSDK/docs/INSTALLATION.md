# Preview and Install a GM Plugin

GM Plugin SDK builds `.gmp` packages. Plugin Open Platform provides two public
development paths:

- use the prebuilt Desktop Studio to run the `.gmp` in a software previewer;
- serve the `.gmp` over the SDK's debug LAN channel and install it on physical
  glasses through the official App.

No separate installer source repository is required.

## Build a plugin

From the `GlassSDK` root, build one example on macOS or Linux:

```sh
python3 build.py build --example game/2048
```

On Windows, open Terminal or Windows PowerShell and run:

```powershell
py build.py build --example game/2048
```

Command Prompt is not supported. The build tool installs missing CMake, Ninja,
RISC-V compiler, and QR dependencies automatically. The generated package is:

```text
build-host/.build/game/2048/2048.gmp
```

After building, this command prints the installation QR code, writes
`2048.qr.png` beside the package, and keeps its transfer server running until
`Ctrl+C` is pressed.

## Preview in Desktop Studio

Keep the public repository layout intact and start the prebuilt application for
your platform under `../Studio/`:

```text
plugin-open-platform/
|-- GlassSDK/
|-- WebSDK/
`-- Studio/<platform>/
```

Desktop Studio recursively discovers manifests under `GlassSDK/examples`. For
each `examples/<relative-path>/manifest.json`, it loads the built package from
`GlassSDK/build-host/.build/<relative-path>/`. Studio does not contain a fixed
list of example names and ignores unrelated files in the build area.
Select the built plugin from the glasses plugin selector, or import an external
`.gmp` file. The software previewer runs the RV32 package, public Host API,
LVGL compatibility layer, virtual 600 x 350 display, buttons, accessory input,
head motion, locale, and plugin logs.

Desktop Studio is a development simulator. It does not reproduce physical
optics, sensor noise, firmware scheduling, Bluetooth timing, or target memory
pressure exactly. Complete final validation on physical glasses.

## Install on physical glasses

### 1. Prepare the connection

The phone and development computer must be on the same trusted LAN. The
official App must already be paired with the glasses. If another host owns the
glasses Bluetooth SPP connection, disconnect it before continuing.

In the official App, use **Settings > Device connection > Unpair device** when
the glasses must be paired again. A factory reset is normally necessary only
when the connection cannot be recovered; it removes the existing pairing.

### 2. Start the GMP QR server

The single-example build command in step 1 has already started this server. To
build incrementally without selecting an example and serve the most recently
updated valid package, run this from `GlassSDK`:

macOS or Linux:

```sh
python3 build.py
```

Windows PowerShell:

```powershell
py build.py
```

To serve an existing package without rebuilding it:

```sh
python3 build.py serve --gmp build-host/.build/game/2048/2048.gmp
```

Use `py build.py` on Windows. The command prints a QR code and writes a
`*.qr.png` image beside the `.gmp`. The server wraps the GMP in a
single-component developer-app ZIP in memory and serves it until `Ctrl+C` is
pressed; it does not write an extra ZIP beside the build output.

Desktop Studio exposes its complete selected phone/glasses combination through
the same development QR workflow. Use either server, not both on the same TCP
port.

### 3. Scan and run

In the official App, open:

**Settings > Device information > Debug > Developer Workbench > Scan to
import**

Scan the terminal or PNG QR code. The App downloads the package, validates its
size, SHA-256, GMP header, CRC, ABI, and memory bounds, then installs and starts
it on the glasses.

The QR uses this compact URI form:

```text
mmapp+tcp://192.168.1.8:18765
```

Port `18765` is used by default. Use `--port 0` for a random free port or
`--host <address>` when automatic LAN address selection chooses an interface
that the phone cannot reach.

This debug channel has no TLS, authentication, or signature. Use it only on a
trusted LAN with packages from a trusted source. Size, SHA-256, and GMP CRC
detect truncation and inconsistent content but do not prevent an active
attacker from replacing both the package and checksum.

The complete ZIP and incremental update contract is documented in
[Developer App ZIP](../../APP_BUNDLE.md).

## Lifecycle operations

| Operation | Result |
| --- | --- |
| Install | Transfer and validate a `.gmp`; replacing a plugin unloads the previous one |
| Start | Open the installed plugin application on the glasses |
| Stop | Close the visible application while keeping its image loaded |
| Remove | Unload the plugin and release its runtime memory |

Only one plugin is currently supported. Plugins run from temporary RAM and are
not persisted in Flash. After the glasses reboot, serve and install the package
again.

## Troubleshooting

- **Studio cannot find SDK examples:** keep `Studio`, `GlassSDK`, and `WebSDK`
  together in the Plugin Open Platform layout, or import the `.gmp` directly.
- **Phone cannot open the QR address:** confirm that phone and computer are on
  the same LAN, allow the server through the computer firewall, and pass a
  reachable address through `--host`.
- **Package rejected:** rebuild with the current SDK and confirm that the
  firmware supports the package ABI and required extensions.
- **Package too large:** the packer enforces the advertised package and runtime
  memory limits during the build.
- **Transfer interrupted:** restart the QR server if necessary and scan again;
  do not concatenate or resend protocol chunks manually.
- **Plugin disappeared after reboot:** this is expected because plugins are not
  persisted in Flash.
- **Untrusted package:** do not install it. A GMP contains native code and is
  not currently isolated by an MPU. See [SECURITY.md](SECURITY.md).

The transport command and acknowledgement contract is specified in
[PROTOCOL.md](PROTOCOL.md). Most plugin developers should use the SDK QR server
and official App instead of implementing the transport directly.
