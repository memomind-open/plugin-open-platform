# Audio Capture Lab

Audio Capture Lab is the reference Web plugin for the complete native glasses
audio API. It is strictly paired with the `audio_capture_lab` GMP so the
glasses show the same foreground application while capture or playback owns
the audio channel.

The example intentionally provides no browser-microphone fallback and sends no
audio to a server. It exists to expose the exact Host contract:

- `recording` keeps Opus frames in Host memory and returns only a short-lived
  `recordingId` plus duration, frame count, and encoded byte count;
- `stream` exposes `ReadableStream<AudioChunk>` over the SDK's transferable
  binary MessagePort path, never Bridge JSON or Base64;
- `captureState` and `playbackState` events drive both the phone UI and the
  compact glasses status screen;
- a single click on the glasses asks the Web plugin to stop active audio, while
  a long press exits the GMP and lets the Host release the paired runtime.

The default UI shows only capture mode, pickup mode, noise reduction, and the
stream profile. Detailed timing, queue, overflow, backpressure, capability,
and event data remain available in collapsed advanced and diagnostics panels.
Recording mode presents only duration, Opus frame count, and encoded size;
stream-only chunk, queue, drop, and discontinuity metrics stay hidden until
real-time streaming is selected.
English and Chinese are selected from the header; the first run follows the
phone language and the selection is remembered.

## Parameters demonstrated

Common capture options:

- the five active `pickupMode` values, with `frontBalanced` selected by default;
- native `noiseReduction` on/off;
- fixed Opus, 16 kHz, mono format;
- `AbortSignal` cancellation.

Recording mode demonstrates a 1-15 second limit and Host playback using the
original recorded voice.

Stream mode demonstrates `interactive`, `balanced`, `reliable`, and `custom`
profiles. Custom mode exposes 20-200 ms chunks, a 100-5000 ms Host queue,
`drop-oldest`, `drop-newest`, or `error` overflow behavior, and an optional
1 second to 1 hour duration. A diagnostic consumer delay deliberately applies
backpressure so developers can observe queue latency, dropped frames, and
discontinuities.

## Run

Build both halves from the Plugin Open Platform root:

```sh
# Ubuntu/macOS
./build.py

# Windows PowerShell
py build.py
```

In Desktop Studio, select the Audio Capture Lab Web plugin and its automatically
paired Audio Capture Lab GMP. Desktop Studio uses the computer microphone: it
can validate parameter transport, binary streaming, backpressure, state, and
playback. Its `noiseReduction` option maps to the browser microphone constraint,
but it cannot reproduce the physical differences between glasses pickup modes.

Use the Android App with connected glasses for final pickup-mode and native
noise-reduction validation. Starting audio may show Host-owned consent and
recording indicators outside the WebView.

## Architecture boundary

The GMP receives only a 23-byte status packet on channel `0x414C`; audio never
passes through plugin Bluetooth messages. The Web stream consumer discards
each Opus chunk after updating metrics. Replace that isolated consumption point
with an application-owned binary network sink when building real-time
translation or assistant features.
