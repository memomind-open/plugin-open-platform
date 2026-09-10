# Audio Capture Lab

Audio Capture Lab is the reference Web plugin for the complete native glasses
audio API. It is strictly paired with the `audio_capture_lab` GMP so the
glasses show the same foreground application while capture or playback owns
the audio channel.

The example intentionally provides no browser-microphone fallback and sends no
audio to a server. It exists to expose the exact Host contract:

- the Host exposes only `openCapture()` and always returns
  `ReadableStream<AudioChunk>` over the SDK's transferable
  binary MessagePort path, never Bridge JSON or Base64;
- the short-recording tab uses the Web SDK-only `openRecording()` helper, which
  immediately consumes that same stream and returns bounded Opus bytes to H5;
- `captureState` drives native capture UI; playback is ordinary H5 `<audio>`;
- a single click on the glasses asks the Web plugin to stop active audio, while
  a long press exits the GMP and lets the Host release the paired runtime.

The default UI shows only capture mode, pickup mode, noise reduction, and the
stream profile. Detailed timing, queue, overflow, backpressure, capability,
and event data remain available in collapsed advanced and diagnostics panels.
Recording mode presents only duration, Opus frame count, and encoded size;
stream-only chunk, queue, drop, and discontinuity metrics stay hidden until
real-time streaming is selected.
English and Chinese are selected from the header; each run initially follows
the phone language and a manual switch lasts only for that run.

## Parameters demonstrated

Common capture options:

- the five active `pickupMode` values, with `frontBalanced` selected by default;
- native `noiseReduction` on/off;
- fixed Opus, 16 kHz, mono format;
- `AbortSignal` cancellation.

Short recording demonstrates the Web SDK's 1-15 second, 750-frame, and 64 KiB
in-page limits plus H5 playback of the transferred Opus recording. These are
not Host capture modes or Host-side recording buffers.

Real-time capture demonstrates `interactive`, `balanced`, `reliable`, and `custom`
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
