# HUD display control over Bluetooth

A native client can use two public paths: built-in device business services, or
application messages to an already-running GM Web Bridge glasses plugin.
Neither path transfers a GMP file. The official App remains responsible for
provisioning and launching the executable glasses plugin where one is required.

## Select the display path

| Need | Wire path | Prerequisite |
| --- | --- | --- |
| Brightness, vertical position, optical distance | GM system service `01` | Native settings handler |
| Native translation/teleprompter/notification/navigation UI | Corresponding native service | Matching native session and product support |
| Arbitrary text, rectangles, lines, raw/LZ4 image tiles | GM service `0F`, command `28`, Web Bridge application channel | Compatible Web Bridge plugin already running |
| Custom drawing and interaction logic | Same application-message envelope, application-defined channel | Your matching glasses plugin already running |

Changing brightness does not open a plugin scene. Conversely, the Web Bridge
channel numbers are an example application's contract, not global firmware
commands supported by every installed plugin.

## Native display settings

Each request below has exactly one INT8 TLV: `08 00 00 01 value`.
Use the common GM frame and a fresh event ID; expect STATUS, not plugin INT8
status. See [device commands](DEVICE_BUSINESS_PROTOCOL.md) for state queries.

| Service | Command | Decimal value |
| --- | --- | --- |
| `01` | `0E` brightness | 0–10 |
| `01` | `0F` automatic brightness | 0 disabled, 1 enabled |
| `01` | `0A` vertical position | 0–8 |
| `01` | `0B` optical distance | 0–8 |
| `01` | `09` media fullscreen setting | 0 disabled, 1 enabled |

For example, brightness 5 uses:

```text
FA 00 00 0E 01 01 0E 08 00 00 01 05 01 26
```

The [wire appendix](WIRE_EXAMPLES.md) labels **every byte**, including both sum
bytes. The relevant setting may be applied asynchronously on the display task.
A successful ACK is not evidence that a frame was rendered. Query status after
processing and observe the display during device acceptance testing.

Native translation text uses service `03`, provisional `03` or final `04`,
with JSON `[translatedText,sourceText]` following a native start. Teleprompter
and notification schemas are in [DEVICE_BUSINESS_PROTOCOL.md](DEVICE_BUSINESS_PROTOCOL.md).
They have their own sessions and display semantics; do not send arbitrary PNG
bytes into navigation's product-specific image decoder.

## Application-message envelope

A Web Bridge downlink uses:

```text
FA LL LL LL ee 0F 28
07 00 00 02 cc cc
01 nn nn nn [application bytes]
ss ss
```

- `LL LL LL`: logical GM length = 19 + application byte count, big-endian.
- `ee`: one-byte event ID.
- `07 00 00 02 cc cc`: INT16 application channel, big-endian.
- `01 nn nn nn`: BYTES TLV type and 24-bit byte length.
- `ss ss`: additive sum of this physical frame before the sum, modulo 65536.

These are placeholders; use a complete generated vector to test your encoder.
For a fragmented message, every physical frame repeats the seven-byte header
and has its own sum. A TLV or text string can cross a GM fragment boundary;
reassemble before parsing it. The body is **binary**, not Base64. Base64 is only
an internal representation used by the official App's H5 Bridge.

The delivery ACK contains INT8 status and INT32 `next_offset`; status 0 reports
delivery, while `next_offset` is not a display cursor. It does not prove that the
plugin accepted a malformed drawing payload. A plugin business reply uses
command `29`, independent of the original delivery ACK.

## Web Bridge channels 1–6

All multi-byte fields below are unsigned big-endian unless specified. Lengths
are the application BYTES length, excluding GM and TLV overhead.

| Channel | Application payload | Length |
| ---: | --- | --- |
| 1 clear | One `00` compatibility byte; the runtime rejects empty application messages | 1 recommended |
| 2 text | `id:u8 x:u16 y:u16 width:u16 height:u16 border:u8 radius:u8 utf8_text...` | At least 11 |
| 3 rectangle | `id:u8 x:u16 y:u16 width:u16 height:u16 border:u8 radius:u8` | Exactly 11 |
| 4 delete | `id:u8` | Exactly 1 |
| 5 line | `id:u8 x1:u16 y1:u16 x2:u16 y2:u16 stroke:u8` | Exactly 10 |
| 6 raw bitmap | `x:u16 y:u16 width:u16 height:u16 stride:u16 pixels...` | `10 + stride * height` |

### Text and object semantics

The renderer keeps at most 16 live text/rectangle/line objects, selected by their
one-byte IDs. Reuse an ID to update it; delete or clear unused objects. Do not
assume 256 simultaneous objects because the ID field is eight bits. Text bytes
are UTF-8 without a terminating NUL; the renderer allocates an internal
terminator. Avoid embedded NULs, which would truncate the C text string.

For channel 2, the offsets are:

| Offset | Meaning |
| ---: | --- |
| 0 | Object ID |
| 1, 2 | X high, low byte |
| 3, 4 | Y high, low byte |
| 5, 6 | Width high, low byte |
| 7, 8 | Height high, low byte |
| 9 | Border width |
| 10 | Corner radius |
| 11 onward | UTF-8 text bytes |

Text wraps within its box. The channel does not contain font selection, color,
alignment, or a trailing string-length byte. Rectangles are transparent with a
white outline; rectangle border 0 and line stroke 0 are normalized to 1. Use
coordinates within the current display geometry, not a hardcoded assumption
that every product is 600 by 350 pixels.

### Raw GRAY_4 bitmap

Each byte contains two pixels: high nibble is the left/even pixel, low nibble is
the right/odd pixel. Values 0–15 encode grayscale, with 0 black and 15 white.
For width `w`, minimum stride is `(w + 1) // 2`; the raw handler accepts a larger
stride as row padding. Every row contributes exactly `stride` bytes. For odd
widths, the low nibble of the last pixel byte is unused; set it to zero.

The handler checks nonzero width/height, sufficient stride, exact payload byte
count, GRAY_4 display format, nonnegative destination, and destination bounds.
Although X/Y are encoded as u16, the destination is cast to signed 16-bit;
use the nonnegative valid display range. Raw bitmap writes go through the Host
framebuffer in slices and present the result. They are not LVGL objects with IDs.

Example: a 2-by-2 checkerboard at X=0,Y=0 has stride 1 and pixels `F0 0F`:

```text
00 00 00 00 00 02 00 02 00 01 F0 0F
```

Wrap those 12 bytes as channel 6. The appendix includes the complete GM packet.
Do not pass PNG, JPEG, a BMP file header, or an Ogg packet as raw pixels.

## LZ4 tiles and atomic frame updates

| Channel | Application payload |
| ---: | --- |
| 7 | `x:u16 y:u16 w:u16 h:u16 stride:u16 decoded_size:u32 raw_lz4_block...` |
| 8 | `frame_id:u32 tile_count:u16` |
| 9 | `frame_id:u32 tile_index:u16 x:u16 y:u16 w:u16 h:u16 stride:u16 decoded_size:u32 raw_lz4_block...` |

For compressed tiles, stride must equal `ceil(width / 2)` and decoded size must
equal `stride * height`. Use a raw LZ4 block, not an LZ4 frame/file wrapper.
The Host must expose the LZ4 extension. Unframed channel 7 presents immediately.
For an atomic multi-tile update, begin on channel 8 and send channel 9 tiles in
order, waiting for channel `0104` status after begin and every tile. At most
256 tiles are accepted. Only the final tile presents the accumulated image.

Channel `0104` payload is 20 bytes:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 1 | Version 1 |
| 1 | 1 | Event type 5 |
| 2 | 4 | Wrapping sequence |
| 6 | 4 | Wrapping monotonic timestamp in milliseconds |
| 10 | 4 | Frame ID |
| 14 | 2 | Received tile index, or `FFFF` for begin |
| 16 | 2 | Next expected tile index |
| 18 | 1 | Status |
| 19 | 1 | Complete flag, 1 only after final presentation |

Statuses: 0 OK, 1 invalid payload, 2 missing LZ4, 3 decode failure, 4 allocation
failure, 5 framebuffer failure, 6 wrong/inactive frame, 7 wrong tile index,
8 timeout. A frame times out after five seconds without a successfully handled
expected tile. Duplicate earlier tiles are ACKed without redrawing. A new valid
begin replaces the active frame. While a frame is active, channels 1–7 are
consumed without drawing. An interrupted update should be followed by a full
replacement image because already submitted framebuffer slices cannot be
individually rolled back.

These image-tile transactions are public display data, not GMP transfer.
See the [renderer implementation and full input-event contract](../examples/web_bridge/README.md).

## Phone-to-renderer call chain

A native client sends the binary channel/BYTES envelope directly. An H5 client
instead uses `gm.display` or `gm.plugin.sendMessage`, subject to App permissions.
Both converge on the firmware application-message handler. It queues the
message onto the display task, which delivers `GM_PLUGIN_EVENT_BT_MESSAGE` to
the running plugin. `web_bridge/plugin.c` switches on the channel, validates
payload length, decodes big-endian fields, and invokes the public LVGL or
framebuffer Host API. It does not parse HTML or execute phone-supplied drawing
code. Plugin replies and frame status travel back through `host->bt_send`.
