> Custom paired plugin: use the matching device plugin for this example and declare and grant device.messaging channel permissions before communicating. Package admission and controlled messaging are available again. Do not arbitrarily pair this example with the default Web Bridge; hardware functionality still requires validation with its matching device plugin.

# Audio Talking Pet

An original talking-pet game built as a static GM Web Plugin example. Memo can
be petted, fed, played with, and put to sleep. Hold the microphone button to
record a short phrase; Memo repeats it with a playful voice effect. In the App,
the example uses the glasses microphone through `gm.audio`, including native
noise reduction, the Web SDK's bounded short-recording helper, and H5 playback.
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

Open `http://127.0.0.1:4173`. Audio capture always goes through the Host's
glasses-audio API. Desktop Studio supplies its selected computer microphone as
the simulated glasses source; the plugin page never requests browser microphone
permission and never falls back to `getUserMedia` or `MediaRecorder`.

The native flow registers `gm.audio.onCaptureState`, then calls
`gm.audio.openRecording()` with `frontFocus` pickup and noise reduction. This
Web SDK convenience helper consumes the Host's unified stream immediately. Stopping
returns Opus bytes and frame boundaries to H5. The page wraps those packets in
Ogg and plays them with `<audio>` at a higher playback rate for the pet effect.
This simple pet example intentionally does not consume the real-time stream API.

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
