# Paired-plugin application messaging

## Transport layers

H5 plugins use the official App Bridge, not raw Bluetooth sockets or GATT
characteristics. The App sends GM application messages to the running glasses
plugin. The documented phone link uses Bluetooth Classic SPP, or iAP2 on iOS;
these UUIDs are not BLE GATT service/characteristic UUIDs. BLE accessory/HOGP
connections are a separate path and must not be confused with this link.

The binary device contract is in
[GlassSDK PROTOCOL](../../../GlassSDK/docs/PROTOCOL.md#plugin-application-service-0x0f).
Application data uses service `0x0F`, command `0x28` toward the glasses and `0x29`
for plugin-originated data. Its frame, TLV and checksum formats are public.
Executable package delivery remains managed by the official App. Native clients
can implement these business messages directly; see the
[Bluetooth/BLE integration guide](../../../GlassSDK/docs/BLUETOOTH_DEVELOPER_GUIDE.md)
for actual GATT discovery, framing, transport availability, HUD and audio.
H5 Bridge restrictions do not mean the public native wire protocol is withheld.

## Declare and obtain permission

Include these fields in the current manifest (merge with its other fields):

```json
{
  "bridgeVersion": "2.0",
  "schemaVersion": 2,
  "permissionPolicyVersion": 1,
  "permissions": [
    {
      "name": "device.messaging",
      "required": true,
      "scope": { "channels": [17991, 17992] }
    }
  ]
}
```

These decimal channels are `0x4647` and `0x4648`. Declare both request and reply
channels. The user must grant the required scope; declaration alone is not
consent. A send or receive on an unapproved channel is not allowed. Display
channels additionally require `display`; standard input-event channels require
`device.events` covering their event type. Custom messages do not grant access
to unrelated capabilities or permission to install a glasses plugin.

## Send, receive and lifetime

```js
const off = gm.plugin.onMessage(({ channel, data }) => {
  if (channel === 0x4648) console.log(data); // Uint8Array
});
const result = await gm.plugin.sendMessage(
  0x4647, new Uint8Array([2, 1, 0, 0]),
);
console.log(result.sent, result.channel, result.payloadBytes);
// On teardown, when this listener is no longer needed:
// off();
```

The channel is an integer from 0 through 65535. Data is a non-empty Uint8Array
of at most 81,901 bytes. This is a maximum logical business payload, not a BLE
MTU, recommended working-buffer size or installation-block size. Use smaller
application messages when latency and memory matter.

Internally the Bridge uses `{ channel, dataBase64 }`; the SDK handles Base64 and
returns Uint8Array to the listener. Device framing uses opaque binary bytes.
The App filters incoming messages by the active authorization and channel scope;
it drops empty/oversized or disallowed messages. Events carry a runtime
generation and the SDK discards stale-generation events. The event name is
`plugin.message`; it does not need a separate `device.subscribeEvents` call.

A successful send confirms transport delivery to the running plugin, not
completion of its business operation. A reply arrives independently. Define
request IDs, response status, timeout and duplicate handling in your application
payload when needed. Do not interpret a transport ACK as a business response.
The glasses callback borrows payload memory only for the callback duration;
copy bytes that must survive it. No executable package is installed by this API.

## Failure behavior

| Bridge error | Meaning |
| --- | --- |
| `INVALID_REQUEST` | Invalid channel or malformed/empty payload |
| `PAYLOAD_TOO_LARGE` | Encoded or decoded payload exceeds the allowed size |
| `PERMISSION_DENIED` | Missing/revoked permission, stale authorization or channel outside scope |
| `STALE_RUNTIME` | Call belongs to an earlier runtime generation |
| `UNAUTHORIZED` | Runtime session token does not match |
| `NOT_SUPPORTED` | Host does not support the method or policy version |

SDK-side argument validation may reject before a Bridge call. Connection and
device-operation failures can also reject a send; these errors are not the same
namespace as the numeric GM device status. Re-evaluate capabilities and current
permissions after runtime or authorization changes. Do not repeatedly retry a
permission error or blindly replay a non-idempotent business operation.
