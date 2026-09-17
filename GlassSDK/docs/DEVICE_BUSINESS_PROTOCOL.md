# Device business commands

These are existing native App/device GM services, not executable-plugin transfer.
H5 plugins normally use the published Bridge API; a documented wire command does
not imply an H5 permission or method exists for it. All command IDs below are
**relative to their service**. A command byte alone does not identify a function.

Use [GM framing and TLVs](PROTOCOL.md#gm-packet-framing). Multi-byte integer TLVs
are big-endian. JSON TLVs can contain arrays as well as objects; keep field order
for positional arrays. Ordinary responses use the system STATUS namespace,
not the plugin service's INT8 result layout. Several handlers enqueue work on the
display task: success does not prove pixels were rendered or settings persisted.

## System queries and display settings — service `0x01`

| Command | Request | Successful response / meaning |
| --- | --- | --- |
| `0x01` heartbeat | No payload | STATUS 0 |
| `0x02` device information | No payload | STRING containing JSON, not JSON-type TLV |
| `0x06` device status | No payload | STRING containing JSON, not JSON-type TLV |
| `0x09` media fullscreen | INT8, 0 off / 1 on | STATUS; stores the media fullscreen setting |
| `0x0A` screen height | INT8, 0–8 | STATUS for dispatch; display-task validation/application follows |
| `0x0B` optical distance | INT8, 0–8 | STATUS for dispatch; display-task validation/application follows |
| `0x0C` head-up angle | INT8 unsigned value | STATUS; use the target product's configured angle range |
| `0x0E` brightness | INT8, 0–10 | STATUS; current handler rejects values above 10 |
| `0x0F` automatic brightness | INT8, 0 off / 1 on | STATUS; implementation treats nonzero as enabled |

Do not use an old “1 off / 2 on” interpretation for automatic brightness.
An invalid one-byte payload can produce generic ERROR for some legacy setters,
whereas newer validators return INVALID_PARAM. Check nonzero status rather than
assuming every parameter error has one specific code.

The device-info JSON fields are `pver` (protocol version), `cap` (capability
bitmask), `model`, `sn`, `mac`, `name`, `ver` (firmware), `cver` (case), `hver`
(hardware), `fver` (font). Unknown fields should be ignored. Feature-gate by the
actual capability definition; do not interpret an unknown bit as universal BLE
or installer support. A memory failure can return STATUS or an error JSON rather
than the expected complete object.

Common device-status fields:

| Fields | Meaning |
| --- | --- |
| `bat`, `charging` | Glasses battery and charging state |
| `pos`, `dist`, `angle` | Height, optical-distance level, head-up angle |
| `brt`, `autobrt` | Brightness level and automatic-brightness setting |
| `lang`, `font` | Firmware language/font settings, not arbitrary ISO language IDs |
| `wear_det`, `dnd`, `ktone`, `wear_tone` | Wear detection, do-not-disturb and sound settings |
| `light_ut`, `light_dt` | Display timeout and head-down delay settings |
| `media_full`, `media_slot_auto` | Media display settings |
| `c_bat`, `c_charge`, `c_status`, `c_g_status` | Case-related fields; current no-case product returns zero |

Status contains additional product fields. Do not infer unsupported sensors from
placeholder zero values. For settings applied asynchronously, query the relevant
state after the display task has processed the request; an immediate read can
still observe the old value.

Brightness level 5, event 1, complete frame:

```text
FA 00 00 0E 01 01 0E 08 00 00 01 05 01 26
```

This is a 14-byte GM frame: service 1, command 14, INT8 value 5, checksum 0x0126.
It does not start, install or replace a plugin.

## Translation — service `0x03`

| Command | Direction | Payload |
| --- | --- | --- |
| `0x01` simultaneous translation start | App → glasses | STRING description/title |
| `0x02` simultaneous translation stop | App → glasses | No payload |
| `0x03` provisional result | App → glasses | JSON array `[translatedText, sourceText]` |
| `0x04` final result | App → glasses | Same ordered JSON array |
| `0x05` conversation translation start | App → glasses | STRING description/title |
| `0x06` conversation translation stop | App → glasses | INT8 mode: 0 exit, 1 background |
| `0x07`, `0x08` provisional/final conversation result | App → glasses | JSON `[translatedText, sourceText]` |
| `0x09`, `0x0A` pause/resume | App → glasses | No payload |
| `0x0C` recording source | App → glasses | Two INT8 TLVs: source (0 glasses / 1 phone), direction (0 front / 1 omni); phone requires omni |
| `0x16` local pause/resume request | Glasses → App | INT8: 0 pause / 1 resume |

The result array is **translation first**, even when the caller's source-code
function accepts source text first. Its lengths are UTF-8 byte lengths in the
outer TLV. The `0x16` event must not be confused with ACKs for `0x09`/`0x0A`,
otherwise an ACK can trigger a pause/resume feedback loop. Start/stop queue
pressure can return BUSY; do not mark the session active before checking ACK.
Actual microphone bytes use the recording transport, not these text messages.

## Teleprompter — service `0x04`

| Command | Direction | Ordered payload |
| --- | --- | --- |
| `0x01` start | App → glasses | JSON `[width,height,cachePages,mode,fontSize,lineWidth,scene]` |
| `0x02` stop | App → glasses | No payload |
| `0x03` page request | Glasses → App | Integer: 1 previous/up, 2 next/down |
| `0x04` display update | App → glasses | INT8 displayMode, INT8 percentage, JSON `[highlightStart,highlightEnd]` |
| `0x05` text content | App → glasses | INT8 contentMode, JSON array of strings |
| `0x06` highlight | App → glasses | JSON `[highlightStart,highlightEnd]` |
| `0x07` mode | App → glasses | INT8: 1 manual, 2 auto, 3 AI follow |
| `0x08` font size | App → glasses | INT8: 1 small, 2 large |
| `0x09` line width | App → glasses | INT8: 1 narrow, 2 medium, 3 wide |
| `0x0A`, `0x0B` pause/resume | App → glasses | No payload |

Start `scene` is 0 preview / 1 actual. `displayMode` is 1 initial, 2 up, 3 down,
4 start from a middle position; `percentage` is 0–100. `contentMode` is 1 initial,
2 prepend, 3 append. These are different mode namespaces. Do not use byte offsets
from the GM packet as text highlight indexes. The App's pagination/text indexing
must match the renderer. A sent highlight ACK is not proof that the UI has
finished displaying it.

## Notifications — service `0x05`

`0x01` carries INT8 style (1 detailed / 2 compact), followed by JSON with:
`id` (notification ID), `a` (App name), `type` (0 App / 1 simulated / 2 schedule),
`ts` (App-local timestamp convention), `c` (content), `ti` (sender/title),
`pkg_name` (source package). The native sender uses wall-clock seconds adjusted
by its zone offset for `ts`; do not silently reinterpret it as UTC milliseconds.

`0x03` changes the notification switch with INT8 0/1. `0x04` changes scroll speed
with INT8 0–2. Both return STATUS after settings validation. **`0x02` is retired**
and returns INVALID_CMD; do not use it as a second notification-push path.

## Navigation — service `0x06`

| Command | Request / behavior |
| --- | --- |
| `0x01` start | JSON positional layout array; current App sends `[0,1,216,0,168,102,0,176,600,160]` for its product layout |
| `0x02` stop | No payload |
| `0x03`, `0x04` large/small map image | BYTES in the matching navigation image codec; not arbitrary PNG/JPEG |
| `0x06` navigation state | JSON serialized navigation-state object |
| `0x10` screen control | INT8: 1 small-map mode, 2 panoramic mode; panoramic requires product support |

The display handler ignores unsupported panoramic mode and rejects other mode
values locally; the earlier dispatch ACK does not prove the switch was applied.
The layout array is a product example, not a portable geometry contract. Image
bytes and state JSON must match the native renderer. Serialize state/image
updates within the active navigation session; cancel old-session image work on
stop or route replacement. Enqueue ACK does not establish that a new map was
rendered. These native commands do not replace the public plugin drawing API.

## Scope and versioning

This page specifies the verified common business paths above. Product-specific
AI/taxi/layout schemas and hardware-specific settings need their corresponding
renderer contract; an enum entry alone is not enough to implement them. Unknown
commands can return INVALID_CMD, and optional compiled-out features can return
NOT_SUPPORTED. Check responses and do not present a feature as working solely
because its command number exists.
