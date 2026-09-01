# Final `.mmpkg` Plugin Package

A Web plugin must be delivered to the App as an `.mmpkg`. The `.mmpkg v1`
format is a ZIP container whose plugin files live directly at the archive root;
do not wrap them in an additional project directory.

```text
example-1.0.0.mmpkg
├── manifest.json
├── index.html
└── assets/
    ├── index.js
    └── index.css
```

An `.mmpkg` is not the same format as the `.gmp` installed on the glasses. The
phone App installs an `.mmpkg` and runs it in a WebView; a `.gmp` is a native
glasses plugin.

## Source manifest

The H5 output directory must contain `manifest.json` at its root. Developers
maintain the business fields; the packager generates `schemaVersion` and
`files`:

```json
{
  "id": "com.example.weather",
  "name": "Weather Plugin",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": ["display", "device.events", "storage"]
}
```

Field constraints:

- `id`: reverse-domain identifier, at most 128 characters, for example
  `com.example.weather`.
- `name`: non-empty display name, at most 80 characters.
- `version`: semantic version such as `1.0.0` or `1.0.0-beta.1`.
- `entry`: package-relative HTML path, at most 256 characters. It must not
  contain empty path segments, `.`, `..`, backslashes, or an absolute path.
- `bridgeVersion`: currently fixed at `1.0`.
- `permissions`: at most 16 unique entries. The allowed values are `display`,
  `device.events`, `storage`, `network`, and `audio.capture`.

Permission meanings:

| Permission | Capability |
| --- | --- |
| `display` | Create, update, and close glasses display pages |
| `device.events` | Subscribe to button, head-motion, connection, and IMU events; read-only `device.getInfo` does not require this permission |
| `storage` | Use App key-value storage isolated to the current plugin |
| `network` | Declare that the plugin needs network access; the current Debug App does not yet enforce a domain sandbox |
| `audio.capture` | Capture bounded Opus audio from the glasses after native user consent and use local playback effects |

The App checks `display`, `device.events`, `storage`, and `audio.capture` permissions for the
corresponding Bridge calls. Do not declare unused permissions. A networked
plugin must still configure a strict CSP; the `network` declaration does not
mean that the App has completed network isolation.

`plugin.sendMessage` uses the device plugin currently installed and running by
the App. It requires no manifest permission by default. It does not install or
select a `.gmp`, and it does not bypass device connection state or protocol
acknowledgements.

## Device plugin requirements

A Web plugin may use `deviceRequirements` to declare required device protocols.
Desktop Studio uses these declarations to pair compatible Web and glasses
plugins before launch:

```json
{
  "deviceRequirements": {
    "preferredPluginId": "com.gm.example.web-bridge",
    "protocols": [
      { "id": "gm.scene", "minVersion": "1.0" },
      { "id": "gm.device-events", "minVersion": "1.0" }
    ]
  }
}
```

- `protocols`: required protocol list, with at most 16 entries. Every version
  provided by the device plugin must be at least its `minVersion`.
- `preferredPluginId`: preferred device plugin when multiple compatible
  implementations exist; it is not a strict ID binding.
- `requiredPluginId`: strict device plugin ID for pairings that cannot use a
  compatible replacement.
- `minPluginVersion`: minimum numeric version for `requiredPluginId`.

Protocol IDs use lowercase letters, digits, periods, and hyphens. Versions
contain one to four numeric components. A device `.gmp` manifest declares its
protocols through `provides.protocols`. In a complete Plugin Open Platform
release, see `GlassSDK/PROTOCOL_COMPATIBILITY.md` for public protocols and the
device-side format.

## Packaging commands

Build a deployable directory with the H5 toolchain, then run the SDK packager:

```sh
npm run build

node /path/to/WebSDK/tools/build-mmpkg.mjs \
  ./dist \
  ./release/weather-1.0.0.mmpkg
```

Inside this SDK repository, you may also run:

```sh
npm run pack:plugin -- \
  /absolute/path/to/plugin/dist \
  /absolute/path/to/release/plugin.mmpkg
```

The input must be the final H5 output, not a project root containing `src`,
tests, or `node_modules`. The output must not be inside the input directory and
must use the `.mmpkg` extension.

The packager:

1. Validates the manifest, entry file, permissions, and paths.
2. Rejects symbolic links and files that exceed resource limits.
3. Computes SHA-256 for every regular file except `manifest.json` and
   `signature.sig`.
4. Writes `schemaVersion: 1` and the complete `files` hash table.
5. Produces a deterministic ZIP and writes the `.mmpkg` atomically.

The final manifest resembles:

```json
{
  "schemaVersion": 1,
  "id": "com.example.weather",
  "name": "Weather Plugin",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": ["display", "device.events", "storage"],
  "files": {
    "assets/index.css": "sha256:<64 lowercase hexadecimal characters>",
    "assets/index.js": "sha256:<64 lowercase hexadecimal characters>",
    "index.html": "sha256:<64 lowercase hexadecimal characters>"
  }
}
```

`files` must cover every payload file exactly. During installation, the App
recomputes each hash and rejects any mismatch.

## Package limits and security boundary

- Maximum `.mmpkg` size: 10 MB.
- Maximum total extracted size: 30 MB.
- Maximum individual file size: 10 MB.
- Maximum regular files: 500.
- Symbolic links, duplicate paths, absolute paths, and path traversal are
  forbidden.
- `signature.sig` is reserved for signatures and is excluded from `files`.

The current App supports unsigned local sideloading only in Debug builds.
Trusted Release keys, review signatures, the plugin marketplace, online
updates, and revocation are not yet complete. Unsigned packages must not be
distributed as production releases.

## App behavior after installation

After an `.mmpkg` is selected, the App validates the ZIP, manifest, resource
limits, and every file hash, then installs it atomically into private App
storage. The same `id` and `version` replace that installed version. An
installed plugin takes precedence over an App-bundled demo with the same ID;
uninstalling it restores the bundled version.
