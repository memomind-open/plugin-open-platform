# Fuguang OS · GM Device-First Plugin

Fuguang OS is a lightweight device system designed around the 600×350 GRAY_4
glasses display. The H5 page is used only to configure the city, notes, and
device state; the complete desktop and application interaction appear on the
glasses.

## Device applications

- Desktop: time, date, weather summary, and six application entries.
- Weather: current conditions, six-hour temperature trend, precipitation, wind
  speed, and humidity.
- Cyclones: active tropical cyclones from GDACS, with alert level, coordinates,
  and radar view.
- Calculator: a two-dimensional device keyboard and the four basic arithmetic
  operations.
- Focus: 25-, 5-, and 50-minute modes with circular progress.
- World clock: Shanghai, Tokyo, London, and New York.
- Notes: quick content saved from the phone console.

## Device controls

- Turn left or right: move horizontally, change the timeline, or select a mode.
- Head up or down: move between rows or change the selected cyclone.
- Single click: open an application, confirm a key, start, or pause.
- Double click: return to the desktop from any page.
- Long press: refresh data, clear the calculator, or reset the timer.
- Nod: confirm. Shake: return to the desktop when supported by the device.

## Run in WebSDK Browser Studio

```sh
# Run from the WebSDK workspace root.
node tools/run-browser-studio.mjs --plugin examples/life-desk
```

## Package

```sh
# Run from the WebSDK workspace root.
npm run pack:plugin -- examples/life-desk dist/life-desk.mmpkg
```

## Implementation

The device image is drawn on an offscreen Canvas and quantized to GRAY_4. Dirty
detection retains six fine-grained 200×175 blocks, but changed blocks in the
same display row are merged into one LZ4 transfer tile. A full-screen update is
therefore sent as two 600×175 tiles instead of six independent requests. A
single dirty row uses the immediate LZ4 channel without a redundant frame-begin
round trip. When both rows change, a Host with atomic-frame support creates one
`frameId`, waits for every device status acknowledgement, and presents the full
image only after the final tile, avoiding row-by-row appearance. Consecutive
operations within 60 ms are coalesced.
After an atomic frame begins, its current image is sent completely and later
state is deferred to the next frame. Older Hosts continue to use the compatible
Channel 7/6 path.

The focus timer updates its digits every second. The countdown is kept within a
single transport tile, while circular progress advances every five seconds, so
per-second updates do not accumulate Bluetooth requests across multiple tiles.
All icons, cards, line charts, circular progress, and selection states are
rendered on the device display rather than in the H5 DOM.

Weather data comes from Open-Meteo and global tropical-cyclone data from GDACS.
Disaster information is for reference only; follow official local warnings.
