# Preview and Install a GM Plugin

GM Plugin SDK builds `.gmp` packages. Plugin Open Platform provides two public
development paths:

- use the prebuilt Desktop Studio to run the `.gmp` in a software previewer;
- use Desktop Studio to package the selected `.gmp`, generate its QR code, and
  install it on physical glasses through the official App.

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
and the RISC-V compiler automatically. The generated package is:

```text
build-host/.build/game/2048/2048.gmp
```

The command prints the generated `.gmp` path and exits. QR generation and LAN
delivery are owned by Desktop Studio.

## Preview in Desktop Studio

Keep the public repository layout intact and start the prebuilt application for
your platform under `../Studio/`:

```text
plugin-open-platform/
|-- GlassSDK/
|-- PhoneSDK/
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

The phone and development computer must be on the same trusted LAN for package
import. Connect the official App to the glasses before starting the glasses
plugin; pairing alone does not establish an active connection.

### 2. Generate the package QR in Desktop Studio

Start Desktop Studio, import the Plugin Open Platform workspace, and refresh
the glasses plugin list. Select the generated `.gmp`; for a glasses-only test,
leave the phone plugin unselected. Studio assembles the selected phone/glasses
combination into one developer-app ZIP and displays its single QR code.

Studio updates the served package whenever the selected combination changes.
The GlassSDK build command does not create a ZIP, QR image, or LAN server.

### 3. Import and run through the official App

In the official App, open **Settings > Memo Lab > Developer Workbench** and use
**Scan to open app**.
Scan the QR code shown by Desktop Studio. The App downloads and validates the
bundle, prepares the phone plugin and manages delivery and startup on the glasses.

The scan page also supports manual entry of the Studio IPv4 address and port
(1–65535). The last address is saved and can be reused or edited, so a camera is
not required for manual import. Enter only the address and port; do not type a
URI prefix. Studio must be running and serving the desired package at that
address. A saved address does not identify an immutable package: Studio serves
the currently selected combination.

The phone and computer must be able to reach the address shown beside the QR
code. LAN package download is not protected by TLS or publisher signatures;
use a trusted LAN and trusted packages. The App still applies its account and
permission checks. Package checksums detect inconsistent content, not a trusted
publisher. See [SECURITY.md](SECURITY.md).

Once imported, the App retains the package for subsequent launches. Reopening
that installed entry does not require rescanning or keeping Studio online.
Reimport after changing a package in Studio to update the App's installed copy.

## Lifecycle and Flash cache

| Operation | Result |
| --- | --- |
| First launch or changed package | The App validates the package and manages transfer to the glasses cache before execution |
| Launch with a complete cache match | Reuses the cached image without transferring the package again |
| Stop | Ends the visible application cycle; the image may remain loaded and its Flash cache is retained |
| Replace the loaded plugin | Stops and unloads the current runtime before loading its replacement |
| Reboot | Clears runtime RAM state; valid Flash cache entries can be reused on the next App launch |

Multiple plugins can be cached, but only one is loaded at a time. Code and
ordinary read-only constants run from Flash; writable data, BSS, relocatable
pointer tables and dynamic allocations need RAM. The build enforces a 500 KiB
Flash code/constants limit and static RAM strictly below 100 KiB, not a 500 KiB
limit on the whole package or a 100 KiB total runtime memory guarantee. See
[ABI.md](ABI.md#runtime-and-memory).

The cache manages space and directory entries, evicts eligible least-recently-used
external plugins when necessary, and keeps an old complete version until a
replacement is validated. An update therefore needs temporary space for its new
candidate. Insufficient capacity is reported through the App. Name, version,
content identity and ABI must match for reuse; cached packages are not selected
solely by name. See [PROTOCOL.md](PROTOCOL.md#flash-cache-and-execution) for the
cache rules, protection policy and interrupted-transfer behavior.

## Troubleshooting

- **Studio cannot find SDK examples:** keep `Studio`, `GlassSDK`, and `PhoneSDK`
  together in the Plugin Open Platform layout, or import the `.gmp` directly.
- **Phone cannot open the address:** check the current Studio address, LAN
  reachability and computer firewall. Edit the saved address if it has changed.
- **Camera unavailable:** use the scan page's manual IP address and port entry.
- **Package rejected:** rebuild with the current SDK and confirm that the
  firmware supports the exact package ABI and required extensions. A changed
  firmware or invalid cache may require the App to send its package again.
- **Package too large or insufficient capacity:** distinguish a build limit from
  device cache capacity. Multiple cached packages and an update candidate share
  the available space. See [ABI.md](ABI.md) and [PROTOCOL.md](PROTOCOL.md).
- **Transfer interrupted:** reconnect the App and retry the installed entry.
  Matching valid candidates can resume from the saved durable checkpoint;
  otherwise the App transfers again. Incomplete images never run.
- **First launch after reboot or firmware update:** a valid cache can be reused;
  an invalid or evicted entry is supplied again by the App. Rescanning is only
  needed when importing a package, not for every glasses reboot.
- **Untrusted package:** do not install it. A GMP contains native code and is
  not currently isolated by an MPU. See [SECURITY.md](SECURITY.md).

Installation and lifecycle control belong to the official App. Public plugin
messages and capability usage are described in [PROTOCOL.md](PROTOCOL.md);
plugin code should not implement a separate device installer.
