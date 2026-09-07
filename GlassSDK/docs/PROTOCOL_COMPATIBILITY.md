# Web and device plugin compatibility

MemoMind Desktop Studio matches a Web plugin to a device plugin through declared
protocols. This avoids coupling ordinary Web plugins to one device package
when multiple packages implement the same transport contract.

## Device plugin declaration

A device plugin manifest may advertise protocols under `provides.protocols`:

```json
{
  "id": "com.gm.example.web-bridge",
  "name": "GM Web Bridge",
  "version": 1,
  "abi_version": 256,
  "provides": {
    "protocols": [
      { "id": "gm.scene", "version": "1.0" },
      { "id": "gm.device-events", "version": "1.0" }
    ]
  }
}
```

Protocol IDs use lowercase dotted or hyphenated names. Versions contain one to
four numeric components. A provider is compatible when its version is greater
than or equal to the Web plugin's minimum version.

Current public protocol IDs are:

| Protocol | Meaning |
| --- | --- |
| `gm.scene` | Scene text, geometry, raw GRAY_4 bitmap and ping channels |
| `gm.scene-lz4` | Independent LZ4 GRAY_4 tile channel |
| `gm.scene-atomic-frame` | Framed LZ4 tiles with stop-and-wait status ACKs |
| `gm.device-events` | Button, IMU and connection event uplink |
| `gm.audio-lab` | Audio Capture Lab state and telemetry on channel `0x414C`; audio is not carried by this protocol |
| `gm.fighter-control` | Fighter Arena input snapshots on channel `0x4647` |
| `gm.fighter-events` | Fighter Arena events on channel `0x4648` |

## Matching behavior

Web manifests declare requirements using the `.mmpkg` `deviceRequirements`
field. Desktop Studio prefers `preferredPluginId` when it is compatible. A
`requiredPluginId` is a strict package identity constraint; use it only when a
protocol-compatible replacement is not acceptable. `minPluginVersion` applies
to the required plugin.

Desktop Studio labels discovered device plugins as recommended, compatible or
incompatible. An explicit developer selection is preserved for diagnostics,
but an incompatible pairing is not reported as ready.

GMP v1 does not embed manifest text. Desktop Studio reads protocol metadata
from the source manifest associated with an SDK `build-host/.build` output, or
from a `manifest.json` beside an imported package. A standalone `.gmp` without
either source layout is reported as having unknown compatibility rather than
being treated as incompatible.
