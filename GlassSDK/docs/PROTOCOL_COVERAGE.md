# Business Protocol Documentation Coverage Review (2026-09-17)

## Public scope

The public documentation describes plugin business messages and protocols for other
device features. The official App manages plugin package installation, compressed
chunks, and transfer resumption. Public documentation does not provide the commands
and packet specifications needed to implement a standalone installer.

An interface in source code is not necessarily a complete public contract. This
review uses the current interface registry, handlers, and Markdown documentation;
an enum name alone is not treated as a verified protocol specification.

## Corrections completed in this review

| Area | Finding | Correction |
| --- | --- | --- |
| Bidirectional custom messages | The API reference said messaging was unavailable, while the developer guide said no permission was required; both conflicted with the App implementation | Documented `device.messaging` authorization and channel scope consistently |
| Receive permissions | Callback documentation did not explain inbound channel filtering | Added authorization in both directions, additional permissions for standard display/event channels, and filtering of stale runtime generations |
| Binary data and the Bridge | Base64, business payload bytes, and GM frames could be confused | Distinguished H5 Uint8Array, Bridge Base64, and device binary data |
| Response semantics | A command ACK could be mistaken for completed business processing | Documented separate business responses, request IDs, timeouts, and responsibility for duplicate handling |
| Error codes | Plugin INT8 status, system STATUS, and Bridge string errors were not clearly distinguished | Added numeric plugin errors and Bridge permission, runtime-state, and parameter errors |
| Transport names | An SPP UUID could be mistaken for a BLE GATT characteristic | Clarified that Classic SPP, iAP2, and BLE/HOGP are separate transports |
| Device information queries | Documentation said no permission was required | Corrected the requirement to `device.info` |
| Permission manifests | Legacy string lists and Bridge 1.0 examples remained | Updated the package format page with current object declarations and version requirements |

See the [business protocol](PROTOCOL.md) and
[bidirectional App plugin messaging](../../PhoneSDK/docs/web-plugin/application-messaging.md).

## Additional coverage completed

| Interface family | Coverage added | Documentation |
| --- | --- | --- |
| BLE/HOGP ring gateway | Control/GATT/HID commands and TLVs, receive limits, ACKs and asynchronous results, handle generations, scan/discovery fragmentation, and error handling; corrected legacy claims that ACKs carry JSON/seq and unbond completes synchronously | [BLE accessory](BLE_ACCESSORY_PROTOCOL.md) |
| System status and display | STRING JSON formats for information/status queries, common fields, brightness/height/distance values, and asynchronous application limits | [Device business](DEVICE_BUSINESS_PROTOCOL.md) |
| Translation, teleprompter, notifications, and navigation | Common message fields and ordering, uplink/downlink differences, retired notification commands, actual navigation modes, and capability limits | [Device business](DEVICE_BUSINESS_PROTOCOL.md) |
| Common framing | A reconstructable two-frame binary example, per-frame checksums, logical versus physical lengths, cross-frame TLVs, disconnect/error resets, and retry semantics | [GM protocol](PROTOCOL.md#fragmentation-and-recovery) |
| Location | Three methods, two events, WGS84 fields, foreground lifecycle, timeouts, errors, and examples | [Foreground location](../../PhoneSDK/docs/web-plugin/location.md) |
| App permissions and versions | Individual permission mappings for 32 methods, 10 permission categories, scopes, current object declarations, and Bridge/schema versions | [Capability contract](../../PhoneSDK/docs/web-plugin/capability-contract.md) |
| Audio paths | Corrected the misleading claim in the glasses Host capability matrix that recording is available only through HFP; distinguished PhoneSDK native Opus streams from HFP calls | [Capability matrix](CAPABILITY_MATRIX.md) |

## Coverage and validation

These interface contracts were added based on the current source. They do not
promise to expose every internal firmware debugging or factory command as a stable
SDK. Complex models such as product-specific navigation images/map objects, taxi
services, and HID input rule tables still require separate product/accessory
contracts. Generic BYTES fields or command enums cannot replace those definitions.
These models are explicitly outside the claim of complete documentation.

The review used the App's `plugin_location_adapter/source.dart`,
`plugin_capability_policy.dart`, and business encoders in `common_blue/lib/command`;
the SDK's `permission-policy.js` and `web-sdk`; and the firmware's `gm_package.c`,
`bt_data_handler.c`, `display_main_event.c`, `navi_app.c`, and `app_hogp_relay.c`.
Handlers take precedence over historical designs and outdated comments.

At the time of this review, 62 existing permission-policy, Studio-permission, and
Web SDK tests passed. A temporary Node loader resolved local workspace packages to
avoid unlinked dependencies; repository source and dependency files were not
changed. Binary example lengths and checksums were calculated. This does not
replace timing validation over a real Bluetooth connection.

## Limits of the official-App-only installation guarantee

App permission and channel controls constrain H5 plugins; they do not establish
that the glasses authenticate the installation client. The installation command
dispatch path inspected in this review did not demonstrate independent official
App authentication. This was not a complete security audit of every authentication
path in the Bluetooth stack. Withholding documentation is not evidence that an
unofficial client cannot install packages.

If the product must accept only officially authorized installations, a separate
review must verify trusted installation authorization on the glasses, including
its lifecycle for new installations, resumed transfers, and cached activation.
This is an implementation and security-design concern; this review did not change
that code.

## Public SDK expansion (2026-09-20)

The public scope now explicitly includes third-party native Bluetooth clients,
BLE endpoint discovery, HUD scene/pixel payloads, microphone control and Opus
extraction, native HFP Speak routing, media commands, and the complete HOGP
input-map schema. Only executable GMP delivery/installation transactions remain
withheld. HOGP mapping-table fragmentation is public configuration data.

Start with [the integration guide](BLUETOOTH_DEVELOPER_GUIDE.md). The
[wire appendix](WIRE_EXAMPLES.md) provides complete, synthetic, byte-annotated
vectors generated by a public Python example. Offline checks validate framing,
checksums, fragmented reassembly, invalid inputs, and legacy/compact Opus
extraction. These checks do not replace hardware validation.

Source review identified release gaps: conditional legacy GM GATT registration,
unimplemented recording GATT send/registration in the current XGIMI path, and a
GM transmit frame ceiling that is not negotiated with BLE MTU. The App also uses
a legacy control-service UUID representation distinct from its recording-service
configuration. These findings must be resolved/validated against the release
firmware before advertising universal BLE-only capture. Native HFP is separate
from BLE, and the current Web SDK does not expose a method that forces HFP.
