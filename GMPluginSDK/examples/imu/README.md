# IMU

This example demonstrates both IMU gesture events and raw sensor sampling.
Gestures arrive in `on_event`, while `on_loop` reads the pitch angle and raw
gyroscope axes every 250 ms. The returned sample also contains raw
accelerometer axes, which applications can use when needed. No second sample
queue is allocated.

After changing this example, run `./gm-build` from the SDK root.
