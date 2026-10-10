---
name: memomind-business-protocol
description: Implement or verify MemoMind independent Bluetooth clients, GM business messages, HUD, audio, and HOGP integration. Use for public business protocols and cross-platform messaging, not executable GMP installation transport.
---

# Public business protocols

First determine whether the request uses a native Bluetooth client, the H5 Bridge, or the GlassSDK Host API. Their capabilities and permissions differ; documentation for one layer does not establish that another exposes the same interface.

Start with the [Bluetooth developer guide](../../../GlassSDK/docs/BLUETOOTH_DEVELOPER_GUIDE.md), then read the relevant documents:

| Task | Documentation |
| --- | --- |
| GM framing, TLVs, lengths, and checksums | [PROTOCOL](../../../GlassSDK/docs/PROTOCOL.md), [WIRE_EXAMPLES](../../../GlassSDK/docs/WIRE_EXAMPLES.md) |
| Complete HOGP workflow for an independent client | [HOGP_QUICKSTART](../../../GlassSDK/docs/HOGP_QUICKSTART.md), [BLE_ACCESSORY_PROTOCOL](../../../GlassSDK/docs/BLE_ACCESSORY_PROTOCOL.md) |
| HUD text, graphics, and bitmaps | [HUD_PROTOCOL](../../../GlassSDK/docs/HUD_PROTOCOL.md) |
| Microphone Opus capture and HFP playback | [AUDIO_PROTOCOL](../../../GlassSDK/docs/AUDIO_PROTOCOL.md) |
| Device state and display control | [DEVICE_BUSINESS_PROTOCOL](../../../GlassSDK/docs/DEVICE_BUSINESS_PROTOCOL.md) |
| H5-to-glasses plugin business messages | [application messaging](../../../PhoneSDK/docs/web-plugin/application-messaging.md), [PROTOCOL_COMPATIBILITY](../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md) |

## Integration constraints

Treat BLE GATT, Classic SPP/iAP2, HFP/SCO, and glasses-to-accessory HOGP links separately. A successful BLE connection proves neither that HFP audio is established nor that the firmware exposes a recording channel.

Discover services, characteristics, and their actual properties. Do not hardcode attribute handles or conflate the documented legacy UUID representations. Subscribe to the uplink before sending, and fragment according to the negotiated MTU and platform write limits. ATT write completion is not a GM business ACK.

Check byte order, length semantics, checksums, event IDs, response correlation, and asynchronous event routing against the actual protocol. On disconnection, clear partial frames, pending requests, HOGP handles, and recording sessions. Rediscover and resubscribe after reconnecting.

Public business payloads, HUD pixels, audio, and HOGP data can be implemented from the documentation. Executable GMP installation metadata, transfer blocks, and resume/activation transactions are outside the public integration contract. Use the official App for installation rather than deriving a new public interface from the private installer. A protocol being unpublished does not prove that hardware rejects third-party clients.

## Verification

Use `GlassSDK/docs/examples/bluetooth_wire.py` to check the reference codec interfaces and recalculate every byte, length, and checksum in documented examples. For new parsing logic, verify truncation, invalid lengths, unknown types, fragmentation, and state cleanup after disconnection. Reuse the existing test framework without unnecessarily making automated tests depend on physical devices.

Check implementation coverage and gaps in [PROTOCOL_COVERAGE](../../../GlassSDK/docs/PROTOCOL_COVERAGE.md). State the target firmware version and the links actually verified. Without packet captures or device tests, report only codec and simulation results rather than claiming end-to-end BLE/HFP success.
