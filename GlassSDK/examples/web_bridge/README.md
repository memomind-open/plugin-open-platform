# GM web bridge

This example combines the scene bridge, button input, Bluetooth uplink and IMU
gesture capabilities in one plugin. It is intended for a phone WebView that
owns application state while the glasses plugin renders the scene and forwards
device input.

The existing scene command channels remain identical to
[`scene_bridge`](../scene_bridge/README.md). Web Bridge adds one optional
compressed drawing channel. Every multi-byte integer is big-endian.

## Compressed bitmap channel

| Channel | Payload |
| ---: | --- |
| `7` LZ4 GRAY_4 tile | `x:u16 y:u16 w:u16 h:u16 stride:u16 decoded_size:u32 compressed_data...` |

Channel `7` contains one independent raw LZ4 block. `decoded_size` must equal
`stride * height`, and `stride` must equal `ceil(width / 2)`. The plugin safely
decompresses the pixels with the optional Host LZ4 extension, then uses the same
framebuffer path as uncompressed channel `6`.

If the Host does not expose `GM_PLUGIN_EXTENSION_LZ4`, the plugin ignores
channel `7` while all uncompressed drawing and input features remain available.
Applications that target older firmware should continue sending channel `6`.

## Atomic framed bitmap transfer

Channels `8` and `9` add an application-level, stop-and-wait transfer for a
bitmap split into multiple LZ4 tiles. Earlier tiles are submitted with
`present=false`; only the final tile uses `present=true`, so the display syncs
the complete frame at once.

| Channel | Payload |
| ---: | --- |
| `8` frame begin | `frame_id:u32 tile_count:u16` |
| `9` framed LZ4 tile | `frame_id:u32 tile_index:u16 x:u16 y:u16 w:u16 h:u16 stride:u16 decoded_size:u32 compressed_data...` |

The phone must wait for the channel `0x0104` frame status after a successful
frame begin and after every tile. A transport-level Bluetooth acknowledgement
alone does not mean the plugin decoded or submitted the tile. Start at tile
index `0`, send exactly the `next_index` requested by the plugin, and do not
send the next tile until status `0` acknowledges the current one.

Frame status uses the common 10-byte event header followed by:

| Offset | Field | Type | Description |
| ---: | --- | --- | --- |
| 10 | `frame_id` | `u32` | Frame named by the request |
| 14 | `tile_index` | `u16` | Received tile, or `0xffff` for frame begin |
| 16 | `next_index` | `u16` | Next tile expected by the plugin |
| 18 | `status` | `u8` | Result from the table below |
| 19 | `complete` | `u8` | `1` only after the final tile is presented |

| Status | Meaning |
| ---: | --- |
| `0` | OK |
| `1` | Invalid payload |
| `2` | LZ4 extension unavailable |
| `3` | LZ4 decode failed |
| `4` | Allocation failed |
| `5` | Framebuffer operation failed |
| `6` | Wrong or inactive frame |
| `7` | Wrong tile index |
| `8` | Frame timed out |

The plugin accepts up to 256 tiles and times out a frame if no expected tile is
successfully handled for 5 seconds. A duplicate tile whose index is below
`next_index` is acknowledged without being drawn again, allowing the phone to
retry after losing a frame status message. A new valid frame begin replaces
the current transfer. While a frame is active, unframed drawing channels `1`
through `7` are consumed without drawing to prevent an early presentation or
LVGL interleaving. Ping and device input events remain available.

After an interrupted transfer, the next frame should cover the full intended
image area. Submitted framebuffer slices cannot be cancelled individually and
will remain pending until a later successful final tile presents the frame.
Channel `7` remains the compatibility path for an independent LZ4 tile that
must be presented immediately.

## Uplink event protocol

The channels below use GM plugin service `0x0F`, glasses-to-phone command
`0x29`. Phone-to-glasses scene messages continue to use command `0x28`.

## Event header

Every event payload begins with this 10-byte header:

| Offset | Field | Type | Description |
| ---: | --- | --- | --- |
| 0 | `version` | `u8` | Protocol version, currently `1` |
| 1 | `event_type` | `u8` | Event type matching the channel table below |
| 2 | `sequence` | `u32` | Shared wrapping sequence for all event types |
| 6 | `timestamp_ms` | `u32` | Wrapping monotonic event time |

## Event channels

| Channel | Event type | Event data after the header | Total bytes |
| ---: | ---: | --- | ---: |
| `0x0100` | `1` | `button:u16 action:u16` | 14 |
| `0x0101` | `2` | `gesture:u16 active:u8` | 13 |
| `0x0102` | `3` | `accel_x:i16 accel_y:i16 accel_z:i16 gyro_x:i16 gyro_y:i16 gyro_z:i16 temperature:i16 pitch_degrees:i16` | 26 |
| `0x0103` | `4` | `connected:u8` | 11 |
| `0x0104` | `5` | `frame_id:u32 tile_index:u16 next_index:u16 status:u8 complete:u8` | 20 |

Button, action and gesture values are the stable `GM_PLUGIN_*` values declared
in `include/gm_plugin.h`. Boolean values use `0` for false and `1` for true.

## Direction recognition

On Hosts with `GM_PLUGIN_CAP_IMU_RAW`, Web Bridge derives `HEAD_RAISE`,
`HEAD_LOWER`, `LEFT` and `RIGHT` from raw gyroscope samples. It combines the
rotated horizontal axes as `gyro_x - gyro_y`, selects only the dominant axis,
waits for the opposite return-to-center peak, and requires 100 ms of quiet
samples before rearming. A direction sends `active=1` when triggered and
`active=0` after the centered quiet period.

While raw direction recognition is active, equivalent firmware direction and
timeout gestures are consumed to avoid duplicate uplink events. Firmware `NOD`
and `SHAKE` events are still forwarded. If raw mode is unavailable or cannot be
enabled, Web Bridge automatically falls back to forwarding every firmware
gesture. The channel `0x0101` payload does not change in either mode.

Raw IMU *transmission* remains disabled by default. Set
`WEB_BRIDGE_RAW_IMU_INTERVAL_MS` in `plugin.c` to a non-zero interval to send
samples on channel `0x0102`; internal direction recognition does not transmit
the high-frequency samples.

The plugin sends a connection event only after connection succeeds. A device
cannot send `connected=0` over a transport that is already disconnected, so the
phone must also use its transport-level disconnect notification.

`bt_send()` acceptance does not confirm that the WebView consumed an event.
This example logs and drops an event when the Host rejects the send; it does not
block the display task to retry.

Keep framebuffer tiles separate from active LVGL objects. A later LVGL redraw
may overwrite direct framebuffer pixels in an overlapping region.

```sh
cd GlassSDK
./gm-build build --example web_bridge
```

On Windows PowerShell, run `.\gm-build build --example web_bridge` from the
SDK root.
