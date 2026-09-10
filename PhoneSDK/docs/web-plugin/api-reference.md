> Bridge 2.0 开发分支说明：权限与接口变更以 [权限调试说明](permission-debug.md) 为准。本文旧版字符串权限和旧音频接口不再适用。

# Bridge v1 API Overview

## Runtime

- `runtime.ready`
- `runtime.ping`
- `runtime.getBridgeVersion`
- `runtime.getCapabilities`
- `runtime.getLifecycleState`

## Storage

- `storage.get`
- `storage.set`
- `storage.remove`
- `storage.clear`

Storage is private to the plugin ID. Use it for small JSON-serializable state,
not source file content.

## User-selected files

Plugins declaring `files.user-selected` can import and reopen user-selected
files from App-managed private storage:

- `files.pick`
- `files.list`
- `files.stat`
- `files.openRead`
- `files.getUsage`
- `files.delete`

```js
const picked = await gm.files.pick({
  extensions: ['txt'],
  allowMultiple: false,
});
const file = picked.files[0];

if (file) {
  const opened = await gm.files.openRead(file.fileId);
  const reader = opened.stream.getReader();
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      console.log('received binary bytes', value.length);
    }
  } finally {
    reader.releaseLock();
  }
}

const { files } = await gm.files.list();
const usage = await gm.files.getUsage();
console.log(files, usage.totalBytes, usage.maxTotalBytes);
```

`files.pick` copies accepted files into the App's private directory and returns
stable metadata: `fileId`, `name`, `size`, `importedAt`, and optional
`extension`. A cancelled picker returns `{ files: [] }`. `files.openRead`
returns a `ReadableStream<Uint8Array>` backed by a short-lived, runtime-bound
Host resource. Browser Studio transfers a Host-controlled stream port directly;
native Hosts may use an authenticated resource URL internally. Pass optional
`offset` and `length` values for random-access
ranges. Both must be JavaScript safe integers; `length` must be positive when
provided, and the Host clamps a range that extends past EOF. The SDK consumes
the private resource ticket internally; plugins must
not retain resource URLs or temporary platform content URIs.

File bytes do not pass through Bridge JSON and are never Base64 encoded. A
file deletion, runtime replacement, plugin reload, suspension, or close cancels
both unopened tickets and reads already in progress. Use an `AbortSignal` to
cancel a read that is no longer needed.

The current file capability advertises a 400 MiB total private quota, a 400 MiB
maximum selected file size, binary streaming with range support, and up to 20
files in one multi-select operation. There is no library item-count limit.
Query `runtime.getCapabilities()` and `files.getUsage()` instead of hard-coding
these limits.

## Display

- `display.createPage`
- `display.rebuildPage`
- `display.updateText`
- `display.updateImage`
- `display.updateImageLz4`
- `display.beginFrame`
- `display.updateFrameImageLz4`
- `display.closePage`

The current device profile is 600×350, GRAY_4, at 30 Hz. Text, coordinate, and
image parameters must pass SDK validation. A logical device acknowledgement
does not mean that a user has visually confirmed the result.

The Scene Bridge accepts at most 81,901 bytes in one payload. A Channel 6 raw
GRAY_4 message uses a 10-byte header and may contain up to 81,891 image bytes.
A Channel 7 raw LZ4 message uses a 14-byte header and may contain up to 81,887
compressed bytes; its decoded bitmap must also be no larger than 81,901 bytes.
Split larger images into independent tiles and compress each LZ4 tile
separately. Do not compress a full screen and then split the compressed stream.

To present multiple tiles atomically, call
`display.beginFrame({ frameId, tileCount })`, then call
`display.updateFrameImageLz4` in ascending `tileIndex` order starting at zero.
The Host waits for the device's Channel `0x0104` status acknowledgement after
the Channel 8 Begin and every Channel 9 Tile. Intermediate tiles update only
the back buffer, and the final successful tile presents the complete frame. If
transmission fails, rebuild the entire frame with a new `frameId`; do not skip
the failed tile.

## Device

- `device.getInfo`
- `device.subscribeEvents`
- `device.unsubscribeEvents`

Events include `device.button`, `device.imuGesture`, `device.rawImu`, and
`device.connection`.

`device.getInfo` is a permission-free, read-only connection query. Subscribing
to or unsubscribing from device events requires the `device.events` manifest
permission.

## Custom messages — not available in the first release

`device.messaging`, `plugin.sendMessage`, and `plugin.message` are not public capabilities. Manifests declaring this permission are rejected; raw calls return `METHOD_NOT_FOUND`. Use `display` for graphics and `device.events` for input. The internal Bluetooth transport remains available to the Host, not to H5 plugins.

## Native glasses audio

Plugins declaring `audio.capture` can use the glasses microphone when
`(await gm.runtime.getCapabilities()).audio` is present:

### Primary capture API: real-time binary stream

```js
const capture = await gm.audio.openCapture({
  profile: 'interactive',
  pickupMode: 'frontFocus',
  noiseReduction: true,
});

const consume = (async () => {
  for await (const chunk of capture.stream) {
    // chunk.data is a Uint8Array containing consecutive Opus frames.
    // chunk.frameLengths identifies every frame boundary without parsing Opus.
    // Send the binary bytes to your own real-time service without Base64.
    await uploadAudio(chunk.data, {
      frameLengths: chunk.frameLengths,
      timestampUs: chunk.timestampUs,
      discontinuity: chunk.discontinuity,
    });
  }
})();

// Later, from an independent UI or lifecycle event:
await capture.stop();
await consume; // drains the bounded final queue before completing
```

The SDK exposes `ReadableStream<AudioChunk>`. Audio moves over a dedicated
`MessagePort` as a single transferable binary envelope per chunk, never through
Bridge JSON and never as Base64. The SDK parses that envelope before exposing
the chunk. Every public chunk includes `sequence`, `timestampUs`,
`durationMs`, `frameCount`, `frameLengths`, `droppedFrameCount`,
`discontinuity`, and `queueLatencyMs`. A discontinuity means audio was dropped
under backpressure and the remote decoder or protocol should be informed.
On a normal stop, the Host flushes the already bounded queue before ending the
stream; it does not discard the final audio tail.

`audio.openCapture` is the only native capture primitive. It has no `mode`
parameter and the Host never retains a complete recording. The stop result
contains `sessionId`, `durationMs`, `frameCount`, `opusBytes`,
`deliveredFrameCount`, and `droppedFrameCount`.

### Short recording convenience helper

```js
const recording = await gm.audio.openRecording({
  pickupMode: 'frontFocus',
  noiseReduction: true,
  maxDurationMs: 5000,
});

const result = await recording.stop();
const audio = new Audio(URL.createObjectURL(opusRecordingToOgg(result)));
await audio.play();
```

`openRecording()` is implemented entirely by the Web SDK on top of
`openCapture()`. It immediately consumes the same binary stream using the
`reliable` profile, then returns `data: Uint8Array` and `frameLengths` from
`stop()`. Its in-page buffer is limited to 15 seconds, 750 Opus frames, and
64 KiB. Exceeding a bound cancels the stream with `BUFFER_OVERFLOW`. The Host
does not expose a separate recording mode and does not cache the whole result.

#### Android WebView MessagePort handoff

This is a Host integration detail; plugin code receives only the normalized
`capture.stream`. Because `__memoPluginResolve(response)` carries JSON and
cannot carry a native `MessagePort`, an Android Host returns this additional
field in the successful `audio.openCapture` stream result:

```json
{
  "streamDescriptor": {
    "id": "opaque-random-value-at-least-128-bits",
    "kind": "audio.capture",
    "sessionId": "capture-123",
    "runtimeGeneration": 7
  }
}
```

The Host independently posts a window message to the plugin document. Its data
is the following JSON object (or its JSON string representation), and the
native port is transferred as `event.ports[0]`:

```json
{
  "type": "gm-plugin:stream-port",
  "descriptor": {
    "id": "opaque-random-value-at-least-128-bits",
    "kind": "audio.capture",
    "sessionId": "capture-123",
    "runtimeGeneration": 7
  }
}
```

The two descriptors must match exactly. Delivery order does not matter: the SDK
waits until it has both the Bridge response and the port. A stale runtime,
invalid descriptor, duplicate port, or request timeout closes the port. Desktop
Studio transfers `response.result.streamPort` directly with its existing
`gm-plugin:response` window message; the SDK normalizes both transports to the
same capture session.

Port control messages are JSON strings, not structured-clone objects. Web sends
`{"type":"pull"}` for demand and `{"type":"cancel","reason":"..."}`
for cancellation. Host sends each audio chunk as one complete `ArrayBuffer`,
then the JSON string `{"type":"end"}`, or
`{"type":"error","code":"...","message":"..."}`.

The V1 audio chunk envelope uses unsigned big-endian integers:

| Offset | Type | Field |
| ---: | --- | --- |
| 0 | `u32` | magic `0x474D4155` (`GMAU`) |
| 4 | `u8` | version `1` |
| 5 | `u8` | message type `1` (`CHUNK`) |
| 6 | `u16` | flags; bit 0 is `discontinuity`, all other bits are zero |
| 8 | `u16` | `headerBytes = 40 + frameCount × 2` |
| 10 | `u16` | `frameCount` (1-10) |
| 12 | `u32` | `sequence` |
| 16 | `u64` | `timestampUs` |
| 24 | `u32` | `durationMs` (`frameCount × 20`) |
| 28 | `u32` | `droppedFrameCount` |
| 32 | `u32` | `queueLatencyMs` |
| 36 | `u32` | `payloadBytes` |
| 40 | `u16[]` | one Opus byte length per frame |
| `headerBytes` | bytes | consecutive Opus frame payload |

The buffer length must equal `headerBytes + payloadBytes`, and the frame lengths
must sum to `payloadBytes`. `sessionId` is taken from the already authenticated
capture session and is not duplicated in every audio envelope. Any malformed
envelope terminates the stream with `INTERNAL_ERROR`.

Use one of these stream profiles:

| Profile | Chunk | Host queue | Overflow | Intended use |
| --- | ---: | ---: | --- | --- |
| `interactive` | 40 ms | 200 ms | drop oldest | live translation and assistants |
| `balanced` | 100 ms | 500 ms | drop oldest | ordinary streaming |
| `reliable` | 100 ms | 3000 ms | error | loss-intolerant processing |

Use `profile: 'custom'` to set `chunkDurationMs` (20-200 ms in 20 ms steps),
`maxQueueMs` (100-5000 ms), and `overflowStrategy` (`drop-oldest`,
`drop-newest`, or `error`). A stream is unlimited by default; set
`maxDurationMs` to a value from 1000 through 3600000 when a hard stop is needed.
The default profile is `interactive`, the default pickup mode is
`frontBalanced`, and noise reduction is enabled by default.

Capture is always Opus, 16 kHz, mono, with 20 ms frames. Only one native capture
operation can own the glasses audio channel at a time. Playback is owned by H5.

### State and lifecycle

```js
const offCapture = gm.audio.onCaptureState(console.log);
```

The only native control methods are `audio.openCapture` and `audio.stopCapture`.
Prefer the capture session's `stop()` method. `gm.audio.openRecording()` is a
Web SDK convenience helper rather than another Host method. Capture state is reported through
`audio.captureState`. `audio.playback` has no Bridge methods; it controls whether
the WebView may use H5 media playback. Supported pickup modes, limits, and
stream profiles are advertised directly in the audio capability object; it has
no `modes` branch.

The App shows native consent and a recording indicator outside the WebView.
Hiding, suspending, reloading, or closing the plugin stops audio and closes any
active stream. A plugin may use browser audio only when the Host audio
capability is absent, not as a fallback after a native operation fails.
