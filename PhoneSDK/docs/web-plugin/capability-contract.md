# Current capability and permission contract

The current contract uses `bridgeVersion: "2.0"`, `schemaVersion: 2` and
`permissionPolicyVersion: 1`. Do not copy legacy string permission arrays.
Permissions are objects with `name`, `required`, optional `reason`, and `scope`
for scoped capabilities. Use the same names as the runtime registry below.

## Bridge method registry

A registered method is not necessarily available on every host. Query
`runtime.getCapabilities()`; active authorization, platform support and runtime
state determine effective availability. `null` below means no manifest permission
for that method, not exemption from runtime/session validation.

| Bridge method | Manifest permission |
| --- | --- |
| `runtime.ready` | `null` |
| `runtime.ping` | `null` |
| `runtime.getBridgeVersion` | `null` |
| `runtime.getCapabilities` | `null` |
| `runtime.getLifecycleState` | `null` |
| `storage.get` | `storage` |
| `storage.set` | `storage` |
| `storage.remove` | `storage` |
| `storage.clear` | `storage` |
| `files.pick` | `files.user-selected` |
| `files.list` | `files.user-selected` |
| `files.stat` | `files.user-selected` |
| `files.openRead` | `files.user-selected` |
| `files.getUsage` | `files.user-selected` |
| `files.delete` | `files.user-selected` |
| `display.createPage` | `display` |
| `display.closePage` | `display` |
| `display.updateText` | `display` |
| `display.updateImage` | `display` |
| `display.updateImageLz4` | `display` |
| `display.rebuildPage` | `display` |
| `display.beginFrame` | `display` |
| `display.updateFrameImageLz4` | `display` |
| `device.getInfo` | `device.info` |
| `device.subscribeEvents` | `device.events` |
| `device.unsubscribeEvents` | `device.events` |
| `plugin.sendMessage` | `device.messaging` |
| `audio.openCapture` | `audio.capture` |
| `audio.stopCapture` | `audio.capture` |
| `location.getCurrentPosition` | `location.foreground` |
| `location.watchPosition` | `location.foreground` |
| `location.clearWatch` | `location.foreground` |

## Permission declarations

There are ten recognized permission names: `storage`, `files.user-selected`,
`display`, `device.info`, `device.events`, `device.messaging`, `audio.capture`,
`audio.playback`, `network`, and `location.foreground`. Do not equate the number
of permissions with the number of methods. `audio.playback` and `network` are
host/WebView policies, not raw Bluetooth commands in the Bridge registry.

```json
{
  "bridgeVersion": "2.0",
  "schemaVersion": 2,
  "permissionPolicyVersion": 1,
  "permissions": [
    { "name": "device.info", "required": false },
    { "name": "device.events", "required": true,
      "scope": { "types": ["button", "imuGesture"] } },
    { "name": "device.messaging", "required": true,
      "scope": { "channels": [17991, 17992] } }
  ]
}
```

`device.events` types are `button`, `imuGesture`, `rawImu`, and `connection`.
`device.messaging` channels are integers 0–65535. Declare only the events and
channels actually used; a scope must be nonempty, valid and duplicate-free.
User approvals must remain within the declaration. Both outbound custom messages
and inbound delivery are checked against current approval, including extra
permissions required by standard display/event channels.

Authorization is associated with the current account, installation, package
identity, policy revision and runtime session. Retaining a JavaScript object or
an earlier capability result does not bypass revocation or runtime replacement.
A package declaration is not a grant. Required permissions affect startup;
optional capabilities may be absent or denied and need a usable fallback.

## Layer boundaries

- Web plugins use Bridge APIs. Declaring `device.messaging` does not provide a
  raw socket, arbitrary GATT access, or installer commands.
- Glasses plugins use the native Host ABI. Public device wire protocols describe
  App/device integration; not every wire command has a corresponding H5 method.
- Network/playback declarations do not establish a hardware sandbox for native
  GMP code. Do not infer isolation of arbitrary native memory from H5 permission
  checks or CRC/hash validation.
- Native recording is provided by `audio.openCapture`/`audio.stopCapture`.
  SDK convenience helpers are not additional raw Bridge methods. Read the audio
  contract in [API reference](api-reference.md).

See [application messaging](application-messaging.md), [foreground location](location.md),
[package format](package-format.md) and [permission debugging](permission-debug.md).
