# Audio capture, streaming, and native speaker playback

There are two independent audio paths:

- **Capture:** glasses microphones -> native encoder -> 40-byte Opus packets ->
  recording transport -> native phone client (or App -> H5 stream).
- **Speak:** phone audio source -> phone OS audio session -> native HFP/SCO audio
  route -> glasses speaker. This is not an Opus upload to the recording endpoint.

Control-plane commands and media transport must not be confused. All bytes in
this document are hexadecimal unless a value is explicitly labeled decimal.
Only GMP executable delivery is excluded; audio streaming is public.

## Transport selection

Use the recording SPP UUID `00002024-0000-1000-8000-00805f9b34fb` or iAP2
`ql.iap2.protocol01` on currently implemented paths. The legacy BLE recording
characteristic is `00002025-0000-1000-8000-00805f9b34fb`, but the current XGIMI
branch does not complete its registration/send path. Read the
[BLE implementation status](BLUETOOTH_DEVELOPER_GUIDE.md#current-implementation-versus-the-ble-release-promise)
before implementing a BLE-only client.

Send GM configuration on the **command** transport and raw `52` controls on the
**recording** transport. Neither is a GMP installation operation.

## Configure the microphone before capture

Send GM service `01`, command `2F`, with INT8 `noise_mode` and optionally a
second INT8 `pickup_mode`. Every INT8 TLV is five bytes:

```text
08 00 00 01 vv
^^ ^^^^^^^ ^^
|  |       one unsigned value byte
|  three-byte value length = 1
TLV type = INT8
```

| Field | Decimal values |
| --- | --- |
| `noise_mode` | 0: enable ENC noise reduction; 1: raw path without ENC |
| `pickup_mode` | 0: fixed forward; 1: adaptive three-microphone meeting pickup; 2: suppress wearer/focus on other speech; 3: balanced wearer and forward speech; 4: forward target with side-speech suppression |

Omit the second TLV for compatibility with firmware that does not support pickup
selection. Use the effective settings supported by the target product; the
wire value does not promise a microphone arrangement on every model.
Wait for a **STATUS TLV** with value 0 before starting. Empty fragment ACKs
are not configuration success. The [wire examples](WIRE_EXAMPLES.md) include
complete ENC/forward and raw-mode requests with verified sums.

The App's `PluginAudioAdapter` implements this order: validate authorization,
obtain user consent and exclusive voice ownership, send `01/2F`, initialize the
recording transport, subscribe to decoded Opus frames, then start capture.
A third-party native client must likewise avoid overlapping recording consumers.

## Recording control bytes

These are raw messages, without a GM envelope, TLVs, length field, or checksum.
The third byte is a parameter, **not a payload length**.

| Message | Byte 0 | Byte 1 | Byte 2 | Bytes 3–5 |
| --- | --- | --- | --- | --- |
| Select legacy format | `52` magic | `12` negotiate | `00` legacy | Absent |
| Select compact format | `52` magic | `12` negotiate | `01` compact | Absent |
| Format response | `52` magic | `13` response | Effective mode | `31 28 00`: legacy record size 49, compact record size 40, reserved 0 |
| Start default capture | `52` magic | `01` start | `00` default scene selector | Absent |
| Start notification | `52` magic | `11` parameters | Effective noise mode | Low 24 bits of recording timestamp, big-endian |
| Stop capture | `52` magic | `00` stop | `00` | Absent; no stop ACK |

The firmware validates supported scene IDs; selector 0 falls back to normal
recording. A preset native scene, such as a translation session, can take
precedence. Do not use an undocumented scene value to assume ownership of an
unrelated native capture.

A reference sequence for compact capture is:

```text
command link:    GM 01/2F, wait for successful STATUS
recording link:  52 12 01
recording link:  <- 52 13 01 31 28 00
recording link:  52 01 00
recording link:  <- 52 11 nn tt tt tt
recording link:  <- 52 92 ... Opus data ... (repeated)
recording link:  52 00 00
recording link:  <- drain already queued data, then close the session
```

`nn` and `tt` are placeholders, not literal transmitted bytes. The current App
requests compact format and its parser also handles legacy records; the Studio
reference recording client described in [PROTOCOL.md](PROTOCOL.md#recording-transport)
requests legacy format. Do not use one parser layout with the other format.

The App negotiation deadline is 800 ms. On older firmware without `52 13`,
keep legacy/validated format detection rather than assuming compact succeeded.
Subscribe before sending start because data and start notification can race.
A start timeout is an application deadline, not a three-second timing guarantee
from the firmware. Stop has no ACK; retain the read subscription long enough to
drain queued complete frames, then end the session. Do not invent a successful
stop response. The firmware ignores a Bluetooth stop for recording owned by an
internal App, so a client must not try to stop another consumer's recording.

## Recording packet, byte by byte

Each packet starts with 12 bytes. Unlike GM, this header includes fields whose
producer convention is little-endian. Do not apply GM byte order to all audio
metadata indiscriminately.

| Offset | Size | Example | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | `52` | Recording magic |
| 1 | 1 | `92` | Normal active recording stream |
| 2 | 1 | `00` | Reserved length byte; not GM length |
| 3 | 1 | `00` | Reserved length byte |
| 4 | 1 | `09` | Legacy producer metadata, low byte |
| 5 | 1 | `FF` | Legacy producer metadata, high byte; do not use this as an App request ID |
| 6 | 1 | `08` | Frame count: decimal 1–8 |
| 7 | 1 | `00` | Producer timestamp/metadata byte 0 |
| 8 | 1 | `00` | Producer timestamp/metadata byte 1 |
| 9 | 1 | `00` | Producer timestamp/metadata byte 2 |
| 10 | 1 | `00` | Producer timestamp/metadata byte 3 |
| 11 | 1 | `FF` | Header terminator |
| 12 onward | Variable | See below | Exactly `frame_count` records in negotiated format |

The App describes the producer timestamp as little-endian. Treat it as
producer metadata rather than a portable UTC clock; use frame count/codec
sample duration for audio timing. No checksum follows this recording packet.

| Stream command | Consumer |
| --- | --- |
| `90` | VAD short recording |
| `91` | VAD long recording |
| `92` | Normal recording started by this control path |
| `93` | Call recording |
| `94` | Recording stream switched into the call-recording path |

Do not concatenate `90`–`94` into one audio session. For this normal-capture
example, route only `92` to the consumer and keep other stream types separate.

### Legacy record: 49 bytes per frame

| Relative offset | Size | Meaning |
| ---: | ---: | --- |
| 0–3 | 4 | Legacy prefix/padding |
| 4 | 1 | `28`, encoded Opus byte count = decimal 40 |
| 5–8 | 4 | Legacy codec metadata; **not** part of the Opus packet |
| 9–48 | 40 | One complete Opus packet |

For frame index `i`, Opus starts at `12 + i * 49 + 9`. Copy exactly 40 bytes.
An eight-frame packet is `12 + 8 * 49 = 404` bytes. A common error is feeding
44 or 49 bytes to the Opus decoder because an older source comment calls the
whole suffix "Opus info". The current parser uses offset 9 and length 40.

### Compact record: 40 bytes per frame

No legacy prefix is present. For frame index `i`, Opus starts at
`12 + i * 40`. An eight-frame packet is `12 + 8 * 40 = 332` bytes.
The firmware constructs this representation by copying the same 40 payload
bytes from each legacy record; it does not re-encode them.

### Reader algorithm

1. Keep a byte buffer per recording connection/session. Bluetooth reads can
   split or combine packets.
2. Find magic `52`; inspect the next command byte. Handle the six-byte `11` and
   `13` control notifications separately from audio.
3. For audio, wait for 12 header bytes. Require reserved length bytes `00 00`,
   terminator `FF`, and frame count 1–8.
4. Determine record size from negotiated mode. Wait for
   `12 + count * record_size` bytes, consume precisely that packet, and continue
   parsing any remaining bytes.
5. In legacy mode, validate the per-frame `28` marker and extract bytes 9–48.
   In compact mode, extract consecutive 40-byte packets.
6. Deliver frames only to the matching stream/session. Bound the receive buffer,
   discard incomplete state after disconnect or session replacement, and report
   dropped audio rather than silently pretending it was continuous.

The [reference codec](examples/bluetooth_wire.py) parses complete audio packets
and has legacy/compact synthetic fixtures. It intentionally leaves socket
buffering and session ownership to the native application.

## Decoding, storage, and H5 delivery

The published PhoneSDK capture profile is Opus, mono, 16 kHz input, with 20 ms
frames (320 samples at 16 kHz). Preserve individual Opus packet boundaries. To
produce PCM, feed each packet to an Opus decoder configured for the required
output rate; to save playable Ogg Opus, mux packets with a valid OpusHead,
OpusTags, page CRCs, sequence numbers, and granule positions. Concatenating raw
packets and naming the file `.ogg` does not create an Ogg container.

The public [Web SDK source](../../PhoneSDK/packages/web-sdk/src/index.js)
provides `opusRecordingToOgg`. Its Ogg granule clock is 48 kHz regardless of the
16 kHz input-rate field; packet duration is derived from the Opus TOC. The
PhoneSDK's short-recording wrapper limits in-page buffering; a native client
must define its own bounded buffering and backpressure policy.

Inside the official App, `gm.audio.openCapture()` exposes a runtime-bound binary
stream with frame lengths and timing/drop metadata. This is a **different**
envelope from the 12-byte Bluetooth recording header. The App removes the
Bluetooth record framing before creating the
[H5 binary chunk envelope](../../PhoneSDK/docs/web-plugin/api-reference.md#native-glasses-audio).
`openRecording()` is a Web SDK convenience wrapper, not another firmware command.

## Speak: native HFP playback

For the intended Speak path, generate or decode the audio on the phone and play
it through an established native HFP audio route. The phone acts as the audio
gateway; the glasses act as the hands-free audio endpoint. The OS and Bluetooth
stack negotiate the voice codec and carry audio over SCO/eSCO. Applications do
not write SCO frames into `0x2021` or `0x2025`.

The firmware's HFP SCO-state handler updates connection state and calls
`app_audio_handle_sco_state`. This belongs to the native Bluetooth/audio stack,
not the GM message dispatcher. The recording stream's 40-byte Opus packets do
not define the HFP playback codec, packet length, or speaker sample rate.

A native integration must implement this sequence:

1. Pair/connect the glasses' Classic HFP profile, in addition to any BLE control
   connection. A BLE-connected status alone is insufficient.
2. Configure the phone's communication audio session and request the connected
   Bluetooth communication device. Observe the actual route becoming active.
3. Play the phone-side audio source through that session. Treat route failure
   as failure; do not report speaker success when audio fell back to the phone.
4. Handle interruptions, calls, device disconnects, and audio-focus loss. When
   finished, release the communication route and restore the prior session state.

On Android API 31+, use `AudioManager.getAvailableCommunicationDevices()` and
`setCommunicationDevice()` for the selected Bluetooth communication device,
observe route changes, then `clearCommunicationDevice()` when finished. Older
SCO APIs require a separate compatibility implementation; they are not a BLE
write operation. See the [Android AudioManager contract](https://developer.android.com/reference/android/media/AudioManager#setCommunicationDevice(android.media.AudioDeviceInfo)).
On iOS, an appropriate AVAudioSession using `playAndRecord` and the
[`allowBluetoothHFP` option](https://developer.apple.com/documentation/avfaudio/avaudiosession/categoryoptions-swift.struct/allowbluetoothhfp)
permits HFP routing; verify the actual input/output route and handle interruptions.
These platform pointers are integration guidance, not a claim that the public
JavaScript SDK forces HFP routing.

The current Web SDK has no `speak`, `audio.play`, or native HFP-route method.
H5 `Audio.play()` requests browser playback; `audio.playback` permission does
not by itself establish HFP. If a product requires an H5 Speak button to always
play through HFP, its native Host must supply and validate that routing policy.
A2DP media playback, where supported, is a different native route and must not
be mislabeled as HFP or BLE speaker streaming.

## GM media controls are commands, not audio payloads

Service `01`, command `76` accepts INT8 operation, optionally followed by INT8
volume. The active **handler enum**, rather than an outdated header comment,
defines these values:

| Decimal operation | Meaning | Additional TLV |
| ---: | --- | --- |
| 0 | Play phone media | None |
| 1 | Pause phone media | None |
| 2 | Volume up | None |
| 3 | Volume down | None |
| 4 | Mute music and call volume | None |
| 5 | Set volume | INT8 percentage, 0–100 |
| 6 | Next track | None |
| 7 | Previous track | None |

The current implementation dispatches native media events. Volume-up/down each
send two adjustment events. Explicit volume scales to music 0–127 and call 0–15
using integer division. The request gets a STATUS response, but that only
confirms command handling; it does not confirm sound or force a route. The
handler can even ACK an unknown operation that the inner dispatcher ignores;
clients must validate the operation range themselves.

Command `01/71` sets the AI voice-reply preference with INT8 0/1. It carries no
speech text or audio bytes and does not start playback. Do not present either
`01/71` or `01/76` as a TTS engine or a BLE audio-download command. Complete
play/pause/volume examples are in [WIRE_EXAMPLES.md](WIRE_EXAMPLES.md).

## Acceptance evidence to collect

For a specific release device, record transport, negotiated format, firmware
version, start latency, frame count, sample duration, dropped-frame behavior,
stop tail, repeated starts, disconnect cleanup, and capture contention. For
Speak, separately verify HFP profile connection, active SCO route, actual
speaker output, interruption recovery, and restoration of phone audio afterward.
BLE-only capture remains a release gap until its characteristic registration,
connection tracking, notification transport, and MTU behavior are implemented
and tested on hardware.
