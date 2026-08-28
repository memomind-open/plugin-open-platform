# IMU

This example is a live head-motion debugger for both raw IMU sampling and
firmware gesture events. Its device display refreshes every 50 ms and shows:

- the current head-motion direction derived from the dominant gyroscope axis;
- a detailed nine-pose grayscale portrait selected from smoothed pitch and
  horizontal gyroscope data, with hysteresis to avoid flicker near pose
  thresholds;
- the absolute pitch angle in degrees;
- raw gyroscope and accelerometer X/Y/Z values; and
- the latest firmware gesture and whether it is active or released.

The plugin continues to write a complete raw sample to the Host log every
250 ms so the visual state can be compared with numeric traces. It reads the
Host's latest cached sample directly and allocates no second sample queue.

The source portrait sheet is under `assets/head-motion-sprites.png`. Run
`python build_sprites.py --preview assets/head-motion-sprites-gray4-preview.png`
after replacing it to regenerate the embedded `head_sprites.h` data and inspect
the exact 16-level grayscale result used by the device.

In Studio, select this device plugin and drag the head-motion control. Vertical
movement changes pitch and reports raising/lowering; horizontal movement
reports left/right turning. The Nod, Left, Right, and Shake controls exercise
the independent firmware gesture event path.

After changing this example, run `./gm-build build --example imu` from the SDK
root.
