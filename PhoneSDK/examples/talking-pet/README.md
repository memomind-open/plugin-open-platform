# Memo Talking Pet

An original talking-pet game built as a static GM Web Plugin example. Memo can
be petted, fed, played with, and put to sleep. Hold the microphone button to
record a short phrase; Memo repeats it with a playful voice effect. In the App,
the example uses the glasses microphone through `gm.audio`, including native
noise reduction, Opus streaming, bounded recording, decoding, and playback.
The phone scene combines character motion, props, particles, lighting, and
listening or speaking effects so every interaction has a distinct response.
Feeding, playing, and sleeping use dedicated high-resolution poses. Feeding
and playing share the same prepare, contact, reaction, and recovery timing as
the native glasses animation.

## Run

From `PhoneSDK`:

```sh
node tools/run-browser-studio.mjs --plugin examples/talking-pet
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

- Use the phone buttons for feeding, playing, sleeping, and talking. The
  glasses plugin does not subscribe to IMU gestures.
- Single button press: pet Memo.
- Double button press: feed Memo.
- **Sync to glasses** renders Memo's current stats on the device display.

The pet state is persisted through `gm.storage`. Memo is an original 3D-style
character asset stored locally under `assets/`, with no external network dependency.
Happiness, fullness, and energy each decay by one point every three seconds while
the plugin is open; interactions raise or lower them and immediately sync the
new state to the glasses.

The dedicated `talking_pet` glasses example renders Memo through its
native GRAY4 framebuffer and overlays its text using native LVGL labels. The
Web plugin sends only a seven-byte state packet; it never sends a bitmap. Eight
embedded poses let the glasses animate idle
breathing, blinking, petting, eating, playing, sleeping, listening, and talking
locally, while live status bars and device controls remain synchronized with
the phone.
The package carries a synchronized standalone SDK under `vendor/`, so the same
relative module graph works in both Desktop Studio and the App's isolated
loopback asset server.
