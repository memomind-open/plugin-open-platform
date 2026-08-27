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

## Plugin Message

Use `plugin.sendMessage` to send a custom binary message to the device plugin
currently running on the glasses:

```js
const frame = Uint8Array.of(2, sequence, buttons >> 8, buttons & 0xff);
const result = await gm.plugin.sendMessage(0x4647, frame);
```

Receive a generic binary message sent by the currently running glasses plugin:

```js
const offMessage = gm.plugin.onMessage(({ channel, data }) => {
  if (channel !== 0x4648) return;
  console.log([...data]); // data is a Uint8Array
});

// Remove the listener when it is no longer needed.
offMessage();
```

The Bridge event name is `plugin.message`. Its wire data is
`{ channel, dataBase64 }`; `gm.plugin.onMessage()` validates the channel and
payload and exposes the decoded payload as `Uint8Array`. The event uses the
active `runtimeGeneration`, is not part of `device.subscribeEvents`, and does
not require a manifest permission.

An App Host forwards an uplink by invoking the WebView callback with the same
event envelope:

```js
window.__memoPluginEmit({
  name: 'plugin.message',
  data: { channel, dataBase64 },
  runtimeGeneration,
});
```

- `channel` must be an integer from `0` through `65535`.
- `data` must be a non-empty `Uint8Array` no larger than 81,901 bytes.
- Plugin messaging is available by default and requires no manifest permission.
- A successful send means that the device acknowledged the message and
  delivered it to the running GMP. It does not mean that the GMP completed its
  business logic or display update.

## Native glasses audio

Plugins declaring `audio.capture` can use the glasses microphone when
`(await gm.runtime.getCapabilities()).audio` is present:

```js
const offFrames = gm.audio.onFrames(({ frames, droppedFrameCount }) => {
  // frames contains Opus packets as Uint8Array values. This event is a lossy
  // observation stream; native recording and playback retain every packet.
});
const offState = gm.audio.onState(async (state) => {
  if (state.state === 'stopped') {
    await gm.audio.playRecording({
      recordingId: state.latestRecordingId,
      voice: 'cute',
    });
  }
});
const offPlayback = gm.audio.onPlaybackState(console.log);

await gm.audio.configure({ noiseReduction: true, pickupMode: 'frontFocus' });
await gm.audio.startRecording();
// Later, after an explicit user action:
await gm.audio.stopRecording();
```

Available methods are `audio.configure`, `audio.startRecording`,
`audio.stopRecording`, `audio.playRecording`, and `audio.stopPlayback`. Start,
stop, and playback requests return an operation ID immediately; completion is
reported through `audio.state` and `audio.playbackState`. The Host limits one
recording to 15 seconds, 750 Opus frames, and 64 KiB. Supported pickup modes
and voice effects are advertised in the audio capability object.

The App shows native consent and a recording indicator outside the WebView.
Hiding, suspending, reloading, or closing the plugin stops audio. Desktop Studio
does not advertise this capability; a plugin may use browser audio only when
the capability is absent, not as a fallback after a native operation fails.
