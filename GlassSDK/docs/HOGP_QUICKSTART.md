# Control a HID accessory without the official App

This walkthrough targets an independent desktop or mobile Bluetooth client.
It requires no official App, cloud login, H5 Bridge or executable plugin. The
client controls the glasses with GM packets containing TLVs; the glasses connect
to a BLE HID ring, remote or keyboard and dispatch supported keys locally.

There are two connections. First connect your client to the **glasses** command
service. Then ask the **glasses** to scan/connect the accessory. Scanning for an
accessory with the computer/phone's own Bluetooth API does not perform step two.
The glasses retain their controller-facing role while acting as accessory-side
BLE Central / HID Host; no role-switch command is necessary.

## 1. Establish the controller-to-glasses connection

Use the platform Bluetooth APIs on your computer or phone:

1. Obtain Bluetooth permissions and select/connect the glasses. Complete any OS
   pairing prompt. Discover services rather than using fixed attribute handles.
2. Find the GM service containing write characteristic
   `00002021-0000-1000-8000-00805f9b34fb` and notify characteristic
   `00002022-0000-1000-8000-00805f9b34fb`. The legacy App service UUID is
   `fb349b5f-8000-0080-0010-002020000000`; observe the
   [service-discovery caveat](BLUETOOTH_DEVELOPER_GUIDE.md#phone-facing-ble-discovery).
3. Enable notifications on `0x2022` before sending any requests. This includes
   the OS callback and CCCD subscription (`01 00` if writing the descriptor
   explicitly). Keep notifications enabled for ACKs and unsolicited events.
4. Determine the negotiated characteristic write limit. Send one complete GM
   physical frame per write to `0x2021`, initially using Write With Response.
   Write binary bytes, not an ASCII string containing hexadecimal digits.
5. Send the heartbeat below and require a successful GM STATUS response.

```text
FA 00 00 09 01 01 01 01 06
```

The heartbeat uses event ID `01`, service `01`, command `01`, no TLVs, and
checksum `01 06`. An ATT write completion alone is not a successful GM request.
The firmware's internal "App connected" check represents an active command
transport (including GATT), not a requirement to run the official application.

BLE must be enabled in the target firmware. Service registration is conditional
in the inspected implementation, and long upstream notifications are not sized
dynamically to ATT MTU. Verify both service availability and full scan/HID
notification delivery on your target; a heartbeat alone is insufficient.
See [transport limitations](BLUETOOTH_DEVELOPER_GUIDE.md#current-implementation-versus-the-ble-release-promise).
Where supported by the OS/device, Classic SPP command service
`00007033-0000-1000-8000-00805f9b34fb` is another GM transport; its availability
must not be reported as a successful BLE test. iAP2 requires its own supported
accessory session. The platform adapter is the only platform-specific part of
this workflow; the GM/TLV bytes below are shared.

## 2. Encode requests and parse replies

Accessory control requests use service `10`, command `01` (hexadecimal).
Each contains one JSON TLV:

```text
FA | logical_length:u24be | event:u8 | 10 | 01 |
05 | json_length:u24be | UTF-8 JSON | checksum:u16be
```

For a single-frame request, `logical_length = 13 + json_length`. The checksum is
the sum of all preceding frame bytes modulo 65536. No NUL terminator is needed.
Use a different event ID for each outstanding request. Replace example addresses
and regenerate lengths/checksums; do not modify a copied Hex packet without
updating its checksum.

Use the shipped [Python codec](examples/bluetooth_wire.py) to generate bytes.
From `GlassSDK/docs/examples`, this executable example prints scan frames:

```python
from bluetooth_wire import frames, json_tlv

body = json_tlv({"op": "scan_start", "report": True, "duration_ms": 10000})
# 20 is a conservative downstream write limit, not a guaranteed uplink limit.
for packet in frames(2, 0x10, 0x01, body, frame_bytes=20):
    print(packet.hex(" "))
```

For fragmented requests, serialize the frames and wait for each empty
intermediate GM ACK before sending the next. Each physical frame has its own
header and checksum. Never split a completed frame into arbitrary BLE writes.
See [framing](PROTOCOL.md#gm-packet-framing) and
[write pacing](BLUETOOTH_DEVELOPER_GUIDE.md#ble-frame-size-and-pacing).

On receive, validate and reassemble physical frames before parsing TLVs. The
reference `parse_frame`, `reassemble` and `parse_tlvs` functions implement these
steps for complete physical frames; they do not implement socket buffering or
radio I/O. An incomplete GM body is not yet a JSON document. Consume empty
intermediate ACKs in the fragment sender: they only permit the next fragment,
and must not complete the pending business request or be rejected for lacking
a STATUS/JSON TLV. The final business response completes that request.

Dispatch complete messages as follows:

| Service / command | Interpretation |
| --- | --- |
| `10/01` | Control response: match a pending event ID; STATUS or JSON depending on operation |
| `10/02` | Asynchronous scan batch; read JSON `devices` |
| `10/07` | Asynchronous link/readiness state |
| `10/08` | Bond state, separate from current connection |
| `10/09` | Scan lifecycle (`scanning` or `ended`) |
| `10/06` | Optional forwarded HID input, independent of pending requests |
| `10/04` | Optional private GATT result/data; correlate JSON `seq` |

TLV type `06` is a one-byte STATUS: zero succeeds, nonzero fails; optional type
`02` contains an error string. Type `05` is UTF-8 JSON. `query_cap`,
`query_status` and normally `enable_hid_event` return JSON rather than a success
STATUS. Handle error responses for every operation. Do not confuse command
`06` (HID event) with TLV type `06` (STATUS).

## 3. Scan and connect an accessory

First query capabilities and status. If the desired accessory is already
connected, reuse it and proceed to input verification. If a different accessory
is connected, explicitly disconnect it and confirm completion before scanning;
unbond it if replacing the bond. If a connection attempt or scan is already in
progress, wait for its outcome (or explicitly stop/cancel it) rather than
starting another scan. Do not execute the following table blindly as a script.

Send each applicable JSON below inside a `10/01` request, observing completion:

| Step | JSON body | Expected completion / complete Hex example |
| --- | --- | --- |
| Check support | `{"op":"query_cap"}` | Inspect `scan`, `connect`, `hid`; [Hex](WIRE_EXAMPLES.md#hogp-capabilities) |
| Inspect existing state | `{"op":"query_status"}` | JSON snapshot; [Hex](WIRE_EXAMPLES.md#hogp-status) |
| Scan for 10 seconds | `{"op":"scan_start","report":true,"duration_ms":10000}` | STATUS, then `10/09` and `10/02`; [Hex](WIRE_EXAMPLES.md#hogp-scan-start) |
| Optionally stop scanning | `{"op":"scan_stop"}` | STATUS, then scan end; [Hex](WIRE_EXAMPLES.md#hogp-scan-stop) |
| Connect the selected device | `{"op":"connect","addr":"AA:BB:CC:DD:EE:FF","addr_type":0}` | STATUS, then link progression; [Hex](WIRE_EXAMPLES.md#hogp-connect) |

Put the accessory in its discoverable/pairing mode. The current report-mode
scan merges advertising and scan-response data, then reports only devices with
a nonempty name and a recognized HID service (`0x1812`) or supported HID
appearance. It is not an unfiltered BLE scanner: unnamed HID devices or
devices without recognized HID advertising can be absent even when nearby.

A scan batch has this shape
(synthetic device; optional fields omitted):

```json
{"seq":0,"scan_id":1,"devices":[{"adv_addr":"AA:BB:CC:DD:EE:FF","adv_addr_type":0,"rssi":-48}]}
```

Copy `devices[i].adv_addr` into the connect request's `addr`, and
`devices[i].adv_addr_type` into `addr_type`. Do not substitute the controller's
address or infer the type from the address text. Associate batches with their
`scan_id`; process the separate scan-ended event even if no devices were found.

After connect ACK, wait for link command `10/07`, or query status after a
bounded timeout. Success requires HID readiness, not merely request acceptance.
The current firmware encodes `connected`, `bonded`, `hid_ready`, `gatt_ready` as
JSON numbers `0`/`1`; accept those and boolean equivalents. For example, the
relevant subset of a ready snapshot is `{"state":2,"connected":1,"hid_ready":1}`.
Missing flags must not count as ready. Pairing, encryption, HID discovery,
Report Map parsing and notification subscription run inside the glasses.

Only one accessory is managed. If already connected to the desired accessory,
reuse that state. If a different device is bonded, replacement requires an
explicit unbond operation; do not silently erase a bond when scanning.

## 4. Verify Up, Down, Left and Right

Once HID ready, pressing supported keys on the accessory produces local glasses
input. No custom input map, private GATT subscription or client-side key replay
is needed. Built-in keyboard HID page `07` usages `52`, `51`, `50`, `4F` map to
Up, Down, Left, Right respectively (these usage IDs are hexadecimal).
Actual report byte positions come from the accessory's Report Map.

To observe those events on your computer/phone as well, send:

```json
{"op":"enable_hid_event","enable":true,"raw":false}
```

See the [complete request Hex](WIRE_EXAMPLES.md#hogp-enable-normalized-hid-events).
Receive JSON TLVs on service `10`, command `06`. For normalized `type:"hid_key"`:

| JSON `key` (decimal) | Meaning |
| --- | --- |
| 0 | Up |
| 1 | Down |
| 2 | Left |
| 3 | Right |
| 4 | Select |
| 5 | Back |

`phase` is 0 press, 1 release, 2 repeat, 3 move, 4 cancel. For one action per
press, handle phase 0; only repeat intentionally. Do not reject `key:0` or
`phase:0` as false/missing. The `key` values above are normalized identifiers,
not raw HID usage IDs. Retain `page`, `usage`, `src` and `rid` for diagnostics.

For complete inbound Hex with byte annotations, see [control ACK](WIRE_EXAMPLES.md#hogp-control-success),
[scan result](WIRE_EXAMPLES.md#hogp-scan-result), [HID-ready](WIRE_EXAMPLES.md#hogp-link-ready),
[Up press](WIRE_EXAMPLES.md#hogp-normalized-up-press),
[Up release](WIRE_EXAMPLES.md#hogp-normalized-up-release) and
[disconnected](WIRE_EXAMPLES.md#hogp-link-disconnected). These are synthetic
receive fixtures, not requests to transmit or captured hardware results.

A decoder for an already reassembled body can use:

```python
import json
from bluetooth_wire import parse_tlvs

names = {0: "Up", 1: "Down", 2: "Left", 3: "Right"}

def pressed_direction(service, command, complete_body):
    if (service, command) != (0x10, 0x06):
        return None
    for kind, value in parse_tlvs(complete_body):
        if kind == 0x05:
            event = json.loads(value.rstrip(b"\0").decode("utf-8"))
            if event.get("type") == "hid_key" and event.get("phase") == 0:
                return names.get(event.get("key"))
    return None
```

Local navigation depends on the foreground application handling these input
events. A custom mapping or raw-input subscriber may override default routing.
If mirrored input arrives but the screen does not move, inspect the foreground
app rather than reconnecting blindly. If events are `unknown_usage` or
`vendor_usage`, inspect the descriptor and the optional
[input mapping contract](BLE_ACCESSORY_PROTOCOL.md#input-maps-are-data-not-executable-plugins).
A device advertising HOGP does not by itself prove every report layout is
supported by the current parser.

For accessory-specific data beyond keys, use the optional
[private GATT operations](BLE_ACCESSORY_PROTOCOL.md#private-gatt-operations-0x03):
wait for GATT readiness, query the current capability list, then read/write or
subscribe using its `list_id` and value handle. Parse `10/04` metadata and BYTES;
the accessory vendor defines the meaning of those bytes. None of these steps
is required for supported standard directional keys.

## 5. Disconnect deliberately

Send `{"op":"disconnect"}` on `10/01`. Complete single-frame Hex with event 1:

```text
FA 00 00 20 01 10 01 05 00 00 13 7B 22 6F 70 22 3A 22 64 69 73 63 6F 6E 6E 65 63 74 22 7D 08 07
```

Wait for STATUS acceptance and then `10/07` disconnection (state 4 for an
established connection), or confirm with `query_status` that connected/HID-ready
are cleared. This disconnects **glasses-to-accessory**, retaining its bond.
To delete the bond for replacement, explicitly send `{"op":"unbond"}` and wait
for completion/state confirmation; see [unbond Hex](WIRE_EXAMPLES.md#hogp-unbond).

Closing the computer/phone connection is different: the established accessory
link remains and local input continues. Controller disconnect clears forwarding
and pending controller-driven GATT work and ends report-mode scans. After
reconnecting the controller, resubscribe to GM notifications, query status,
and explicitly re-enable HID forwarding if desired.

## Acceptance and troubleshooting

Run this sequence with the official App closed, and record the controller OS,
firmware build, transport, negotiated MTU and accessory model:

1. Heartbeat succeeds over the intended transport.
2. Scan results arrive with real accessory addresses/types.
3. Selected accessory reaches HID-ready, including pairing when required.
4. Up/Down/Left/Right reach a foreground app that handles them; if mirroring is
   enabled, verify the corresponding normalized press/release events too.
5. An explicit disconnect is confirmed and input from that accessory stops.
6. Reconnect and verify readiness/input again without deleting the bond.

An absent service is a firmware/transport availability issue. A successful
write with no GM reply calls for checking CCCD, framing, checksum and write
limits. A successful heartbeat with missing large scan/HID events calls for
checking upstream MTU delivery. `ERR_ALREADY_BONDED` requires an explicit
replacement decision. Failed pairing, unsupported reports and a non-navigable
foreground page are separate failures; expose them separately to the user.

These instructions and codec vectors are source/offline verified. They are not
a completed radio interoperability test or a promise that documentation alone
fixes transport limitations in a release firmware.
