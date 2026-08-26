# Momo Talking Pet

An original talking-pet game built as a static GM Web Plugin example. Momo can
be petted, fed, played with, and put to sleep. Hold the microphone button to
record a short phrase; Momo repeats it with a playful voice effect. In the App,
the example uses the glasses microphone through `gm.audio`, including native
noise reduction, Opus streaming, bounded recording, decoding, and playback.

## Run

From `WebSDK`:

```sh
node tools/studio-cli.mjs --plugin examples/talking-pet
```

Open `http://127.0.0.1:4173`. Microphone access requires browser permission and
a secure context; `localhost` and `127.0.0.1` are treated as secure contexts by
modern browsers.

Desktop Studio does not advertise native glasses audio, so this example falls
back to `MediaRecorder`. The App path never falls back after a native audio
failure: the user sees the actual device or permission error instead.

The native flow registers `gm.audio.onFrames`, `gm.audio.onState`, and
`gm.audio.onPlaybackState` before starting. It configures `frontFocus` pickup
with noise reduction, calls `startRecording`/`stopRecording`, then plays the
completed recording with the allowlisted `cute` voice effect.

## Device controls

- Single button press: pet Momo.
- Double button press or head-lower/left gesture: feed Momo.
- Head-raise/right gesture: play with Momo.
- **Sync to glasses** renders Momo's current stats on the device display.

The pet state is persisted through `gm.storage`. Momo is an original 3D-style
character asset stored locally under `assets/`, with no external network dependency.

The glasses UI is rendered independently on a 600×350 offscreen Canvas, quantized
to GRAY_4, and sent as six cached 200×175 framebuffer tiles. It includes a compact
star-eyed Momo, live status bars, action feedback, and device control hints.
Its rounded shapes use Canvas path primitives supported by older embedded
WebViews instead of relying on `CanvasRenderingContext2D.roundRect()`.
The package carries a synchronized standalone SDK under `vendor/`, so the same
relative module graph works in both Desktop Studio and the App's isolated
loopback asset server.
