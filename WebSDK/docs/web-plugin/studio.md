# Debugging with Studio

Studio displays the Web page and virtual glasses screen together and can
inject:

- single-click, double-click, and long-press actions;
- head up, head down, turn left, and turn right gestures;
- connection and disconnection events; and
- `running` and `suspended` lifecycle states.

Studio shows the Bridge request log. Error and latency simulation are planned
for a later release; plugins must already handle the structured errors exposed
by the SDK.

Studio also enforces the real Scene Bridge transport limit of 81,901 bytes per
payload. Requests fail with `PAYLOAD_TOO_LARGE` and do not render when the
compressed or decoded size of a Channel 6 raw image, Channel 7 raw LZ4 image,
or Channel 9 atomic-frame raw LZ4 tile exceeds that limit. After Channel 8
starts an atomic frame, intermediate tiles update only the back buffer; the
last tile refreshes the device preview. The preview header reports the channel
and byte count of the most recent drawing operation. If a payload is too large,
split the image into tiles and compress each LZ4 tile independently.
