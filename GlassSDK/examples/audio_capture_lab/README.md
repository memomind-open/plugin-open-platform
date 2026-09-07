# Audio Capture Lab

This lightweight glasses plugin is the required device companion for the
PhoneSDK `audio-capture-lab` example. It keeps the correct application in the
foreground while the Web plugin exercises Host-owned audio capture.

The GMP does not access, encode, decode, retain, or forward audio. The Web
plugin sends a compact versioned state packet on Bluetooth channel `0x414C` at
state changes and at most once per second while streaming. The compact display
shows one large state, one elapsed timer, a one-line capture summary, and a
one-line stream health summary. Its English or Chinese labels follow the Web
plugin language through the reserved language byte in that state packet.

Device controls remain deliberately small:

- single click asks the Web plugin to stop the active capture or playback;
- long press exits the glasses application, which also causes the Host to
  release the associated Web audio runtime;
- Back exits immediately.

Button and connection events use the standard `gm.device-events` uplink wire
format. Audio remains exclusively on the Host/Web binary MessagePort path.
