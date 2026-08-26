# GM Web Plugin Developer Guide

> Status: preview draft
> DevKit version: 0.1.0
> Bridge version: 1.0
> Runtime requirement: Node.js 18 or later

## 1. Overview

A GM Web plugin is an H5 application that runs in the phone App WebView. It can
display its own phone UI and use the GM Web Plugin SDK to:

- draw text and images on the glasses display;
- receive button and head-motion events from the glasses;
- query device connection state;
- exchange custom binary messages with a paired glasses plugin;
- store private plugin data; and
- respond to plugin lifecycle changes.

The preview SDK is distributed as `gm-web-plugin-devkit-0.1.0.zip`. It contains
the browser SDK, Studio simulator, plugin packager, examples, and documentation.
The final distribution will use npm packages without changing the Bridge API,
manifest, or `.mmpkg` format.

## 2. DevKit contents

```text
gm-web-plugin-devkit-0.1.0/
├── README.md
├── DEVKIT-MANIFEST.json
├── package.json
├── sdk/
│   ├── gm-plugin-web-sdk.esm.js
│   └── gm-plugin-web-sdk.d.ts
├── studio/
│   └── gm-plugin-studio.mjs
├── tools/
│   └── build-mmpkg.mjs
├── examples/
│   └── counter/
├── docs/web-plugin/
└── internal/
```

- `sdk/`: browser ES module SDK and TypeScript declarations.
- `studio/`: local simulation of the App, glasses display, device events, and
  lifecycle.
- `tools/`: `.mmpkg` packager for final H5 output.
- `examples/`: runnable projects configured to use the local SDK.
- `internal/`: Studio implementation details that plugins must not import or
  modify directly.

## 3. Run the included example

Extract the DevKit and start Counter:

```sh
unzip gm-web-plugin-devkit-0.1.0.zip
cd gm-web-plugin-devkit-0.1.0
node studio/gm-plugin-studio.mjs --plugin examples/counter
```

Open:

```text
http://127.0.0.1:4173
```

In Studio:

1. Select **Draw to glasses** on the plugin page.
2. Confirm that green text appears on the virtual glasses display.
3. Trigger single-click, double-click, long-press, and head-motion events.
4. Confirm that the plugin page receives the corresponding events.
5. Change the connection and lifecycle state.
6. Inspect requests, responses, and events in Bridge Inspector.

Use another port if `4173` is unavailable:

```sh
node studio/gm-plugin-studio.mjs --plugin examples/counter --port 4174
```

## 4. Create a plugin

A minimal plugin has this structure:

```text
my-plugin/
├── manifest.json
├── index.html
├── app.js
└── vendor/
    └── gm-plugin-web-sdk.esm.js
```

For Vite, Webpack, or another build tool, the final `dist/` directory must
contain the same runtime files and use only package-relative asset paths.

Do not reference:

- absolute paths on the development computer;
- an absolute path to the extracted DevKit;
- files under `node_modules` that were not bundled or copied into `dist`; or
- Studio-only internal URLs such as `/sdk/` or `/runtime/`.

### 4.1 Add the local SDK

During ZIP distribution, copy the SDK into the plugin project:

```sh
mkdir -p ./vendor
cp /path/to/devkit/sdk/gm-plugin-web-sdk.esm.js ./vendor/
```

For Vite, copy it into a static directory that is preserved in the build:

```sh
mkdir -p ./public/vendor
cp /path/to/devkit/sdk/gm-plugin-web-sdk.esm.js ./public/vendor/
```

After building, verify that this file exists:

```text
dist/vendor/gm-plugin-web-sdk.esm.js
```

### 4.2 Initialize the SDK

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();

async function start() {
  await gm.ready();
  console.log('GM Plugin Bridge is ready');
}

start().catch(console.error);
```

`createGMPlugin()` automatically selects `MemoPluginBridge` in the App WebView
or the simulated Bridge in Studio. Business logic does not need separate App
and Studio implementations.

Release SDK resources when the plugin stops:

```js
gm.close();
```

## 5. Configure `manifest.json`

The H5 output directory must contain `manifest.json` at its root:

```json
{
  "id": "com.example.weather",
  "name": "Weather Plugin",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": [
    "display",
    "device.events",
    "storage"
  ]
}
```

| Field | Required | Description |
| --- | --- | --- |
| `id` | Yes | Unique reverse-domain plugin ID, at most 128 characters |
| `name` | Yes | Display name, at most 80 characters |
| `version` | Yes | Semantic version such as `1.0.0` |
| `entry` | Yes | Package-relative entry HTML path |
| `bridgeVersion` | Yes | Currently `1.0` |
| `permissions` | Yes | App capabilities requested by the plugin, at most 16 unique entries |

`entry` must point to an HTML file, be no longer than 256 characters, and must
not be absolute or contain backslashes, empty path segments, `.`, or `..`.

Supported permissions:

| Permission | Capability |
| --- | --- |
| `display` | Create, update, and close glasses display pages |
| `device.events` | Subscribe to button, head-motion, connection, and IMU events |
| `storage` | Use App key-value storage isolated to the current plugin |
| `network` | Declare that the plugin needs network access |
| `audio.capture` | Capture bounded Opus audio from the glasses and play an allowlisted local voice effect |

The App checks `display`, `device.events`, `storage`, and `audio.capture` at the corresponding
Bridge calls. `device.getInfo()` and `plugin.sendMessage()` require no manifest
permission. Do not declare unused permissions.

In the current Debug App, `network` is informational and does not mean that the
App enforces domain isolation. Networked plugins must still use a strict CSP and
connect only to required domains.

See [Final `.mmpkg` package](web-plugin/package-format.md) for
`deviceRequirements` and the complete manifest contract.

## 6. Runtime and storage

Call other Bridge APIs only after `ready()` resolves:

```js
await gm.ready();
```

Runtime queries:

```js
const ping = await gm.runtime.ping();
const bridge = await gm.runtime.getBridgeVersion();
const capabilities = await gm.runtime.getCapabilities();
const lifecycle = await gm.runtime.getLifecycleState();

console.log(ping, bridge.version, capabilities, lifecycle.state);
```

Use Host-reported capabilities instead of assuming that every feature is
available from the SDK version alone.

Storage is private App key-value storage isolated by plugin ID:

```js
await gm.storage.set('game-state', { score: 10, level: 2 });

const result = await gm.storage.get('game-state');
console.log(result.value); // null when the key does not exist

await gm.storage.remove('game-state');
await gm.storage.clear();
```

Store only JSON-serializable values, not DOM objects, functions, or cyclic
references.

## 7. Display API

HTML shown in the phone WebView does not automatically appear on the glasses.
Only content submitted through `gm.display.*` reaches the device display.

Current device profile:

```text
Size: 600 × 350
Refresh rate: 30 Hz
Pixel format: GRAY_4
Studio rendering: 16 brightness levels mapped from black to green
```

All drawing regions must remain fully inside the 600×350 coordinate space,
whose origin is the top-left corner.

### 7.1 Payload limits and tiling

The Scene Bridge accepts at most 81,901 bytes in one payload.

| Channel | Content | Header | Maximum content | Additional constraint |
| --- | --- | ---: | ---: | --- |
| 2 | UTF-8 text | 11 B | 81,890 B | Includes text element parameters |
| 6 | Raw GRAY_4 | 10 B | 81,891 B | `pixels.length = stride × height` |
| 7 | Raw LZ4 GRAY_4 | 14 B | 81,887 compressed bytes | Decoded bitmap must also be at most 81,901 B |
| 8 | Atomic frame begin | 6 B | N/A | `frameId + tileCount`, at most 256 tiles |
| 9 | Atomic-frame raw LZ4 GRAY_4 | 20 B | 81,881 compressed bytes | Send in `tileIndex` order; only the final tile is presented |

A full 600×350 GRAY_4 screen contains 105,000 bytes and cannot fit in one
Channel 6 or Channel 7 request. For a width of 600 and stride of 300:

- Channel 6 fits at most 272 rows, so split the screen into 272 + 78 rows.
- Channel 7 fits at most 273 decoded rows, so split into 273 + 77 rows and
  compress each tile as an independent raw LZ4 block.
- Reduce the tile size again if a compressed tile exceeds 81,887 bytes.

Prefer vertical tiling with a consistent width and stride. Never compress a
full screen and split the compressed bytes; every Channel 7 or 9 request must
contain one independently decodable raw LZ4 block.

### 7.2 Create and close a page

```js
await gm.display.createPage();
// Draw content here.
await gm.display.closePage();
```

### 7.3 Update text

```js
await gm.display.updateText({
  id: 1,
  x: 20,
  y: 20,
  width: 300,
  height: 80,
  border: 1,
  radius: 8,
  text: 'First line\nSecond line',
});
```

- `id` is an element ID from 0 through 255. Reuse it to update the same
  element.
- `x` and `y` are the top-left coordinates.
- `width` and `height` define the element region.
- `border` is the border width; zero disables the border.
- `radius` is the corner radius.
- `text` is non-empty UTF-8 and may contain `\n`.

Studio approximates wrapping and green rendering with browser fonts. Validate
final text layout with firmware fonts on physical glasses.

### 7.4 Update a GRAY_4 image

```js
await gm.display.updateImage({
  x: 0,
  y: 0,
  width: 256,
  height: 256,
  stride: 128,
  dataBase64,
});
```

GRAY_4 uses four bits per pixel. Brightness ranges from 0 through 15; the even
pixel occupies the high nibble and the odd pixel the low nibble. `stride` must
equal `ceil(width / 2)`, and `dataBase64` contains the packed bytes.

### 7.5 Update a raw LZ4 image

```js
await gm.display.updateImageLz4({
  x: 0,
  y: 0,
  width: 256,
  height: 256,
  stride: 128,
  decodedSize: 32768,
  dataBase64,
});
```

`decodedSize` must equal `stride × height`, and `dataBase64` must contain one
raw LZ4 block. Compression reduces transmitted bytes but does not enlarge the
device decode buffer. Tile any decoded image larger than 81,901 bytes before
compressing each tile independently.

### 7.6 Present multiple tiles atomically

```js
const frameId = 42;
await gm.display.beginFrame({ frameId, tileCount: tiles.length });

for (let tileIndex = 0; tileIndex < tiles.length; tileIndex += 1) {
  await gm.display.updateFrameImageLz4({
    frameId,
    tileIndex,
    ...tiles[tileIndex],
  });
}
```

Begin and every tile wait for a device status acknowledgement. `tileIndex` must
increase continuously from zero. Intermediate tiles update only the back
buffer, and the last acknowledged tile presents the complete frame. On any
failure, abandon the frame and resend the full image from Begin with a new
`frameId`. This is an application-protocol status acknowledgement, not a
physical Bluetooth-fragment acknowledgement.

### 7.7 Rebuild a page

```js
await gm.display.rebuildPage([
  {
    type: 'text',
    id: 1,
    x: 20,
    y: 20,
    width: 300,
    height: 60,
    border: 1,
    radius: 8,
    text: 'Hello GM',
  },
  {
    type: 'image',
    x: 320,
    y: 20,
    width: 128,
    height: 128,
    stride: 64,
    dataBase64,
  },
]);
```

Rebuild clears the page and performs the operations in order, with at most 128
operations per call. A logical acknowledgement means that the App or device
accepted the request, not that a user visually confirmed the final result.

## 8. Device events and plugin messages

### 8.1 Device information and subscriptions

```js
const info = await gm.device.getInfo();
console.log(info.connected, info.transport, info.profile);
```

`device.getInfo()` is a read-only query and requires no `device.events`
permission.

Subscribe before registering listeners:

```js
const result = await gm.device.subscribeEvents([
  'button',
  'imuGesture',
  'connection',
]);

const subscriptionId = result.subscriptionId;
```

Supported subscription types are `button`, `imuGesture`, `rawImu`, and
`connection`. Subscribing and unsubscribing require `device.events`.

```js
await gm.device.unsubscribeEvents(subscriptionId);
```

### 8.2 Button events

```js
const offButton = gm.device.onButton((event) => {
  switch (event.action) {
    case 'single':
      console.log('Single click');
      break;
    case 'double':
      console.log('Double click');
      break;
    case 'long':
      console.log('Long press');
      break;
  }
});

offButton();
```

### 8.3 Head gestures and connection

```js
const offGesture = gm.device.onGesture((event) => {
  if (event.active) console.log(event.gesture);
});

const offConnection = gm.device.onConnection((event) => {
  console.log(event.connected ? 'Device connected' : 'Device disconnected');
});
```

Current gesture names are `headRaise`, `headLower`, `left`, `right`, `nod`,
`shake`, `headRaiseTimeout`, and `headLowerTimeout`.

### 8.4 Raw IMU

```js
const offRawImu = gm.device.onRawImu((event) => {
  console.log(event);
});
```

Raw IMU is part of the Bridge contract, but the current browser Studio has no
visual injection panel for it. High-rate IMU increases processing and transport
load; subscribe only when required.

### 8.5 Generic plugin messages

Send binary data to the currently running glasses plugin:

```js
const result = await gm.plugin.sendMessage(
  0x4647,
  new Uint8Array([0x02, 0x01, 0x00, 0x00]),
);

console.log(result.sent, result.payloadBytes);
```

Receive binary data from the currently running glasses plugin:

```js
const offMessage = gm.plugin.onMessage(({ channel, data }) => {
  if (channel === 0x4648) console.log([...data]);
});
```

The Bridge event is `plugin.message` with wire data
`{ channel, dataBase64 }`. The SDK validates the event and returns `data` as a
`Uint8Array`. It is independent of `device.subscribeEvents` and requires no
manifest permission. The Host attaches the active `runtimeGeneration`, and the
SDK discards stale-generation events.

`channel` must be an integer from 0 through 65535. The payload must be a
non-empty `Uint8Array` no larger than 81,901 bytes. Messaging does not install
or switch a GMP. A successful send only confirms the underlying GM command
acknowledgement, not completion of glasses plugin business logic.

See [Bidirectional Plugin Messaging](../../docs/plugin-message-uplink-requirements.md)
for paired Web and glasses examples.

## 9. Lifecycle, sessions, and errors

Lifecycle states are `starting`, `running`, `suspended`, `stopped`, and
`failed`:

```js
const offLifecycle = gm.on('runtime.lifecycleChanged', (event) => {
  console.log(event.state);
});
```

- `starting`: wait for initialization.
- `running`: resume event processing and required display updates.
- `suspended`: pause animation, high-rate work, and unnecessary requests.
- `stopped`: save state and release resources.
- `failed`: stop sending requests and record or show the failure.

Do not assume that a WebView remains resident. Page reload, runtime
reconstruction, or an App state change can invalidate old requests and events.

The Host creates a `sessionToken` and `runtimeGeneration`. The SDK includes
them, plus `version`, `requestId`, `method`, and `params`, in every Bridge
request. These fields isolate plugin sessions, prevent old pages from calling a
new runtime, filter stale events after reload, and match asynchronous responses
to requests. Plugin code should use SDK methods instead of constructing this
envelope manually.

Bridge errors are exposed as `GMPluginError`:

```js
try {
  await gm.display.updateText(params);
} catch (error) {
  console.error(error.code, error.message);
}
```

| Code | Meaning |
| --- | --- |
| `INVALID_REQUEST` | Invalid parameters or request structure |
| `PAYLOAD_TOO_LARGE` | Payload exceeds a transport limit |
| `UNAUTHORIZED` | Session or permission check failed |
| `STALE_RUNTIME` | Request belongs to an old runtime |
| `METHOD_NOT_FOUND` | Method is unknown or unsupported by the Host |
| `RATE_LIMITED` | Request rate exceeds a limit |
| `BUSY` | Device or runtime is busy |
| `AUDIO_BUSY` | Another App audio business owns the glasses recording channel |
| `NO_AUDIO` | Recording stopped without receiving an audio frame |
| `QUOTA_EXCEEDED` | Storage, subscription, or another quota was exceeded |
| `TIMEOUT` | SDK or Bridge request timed out |
| `DEVICE_DISCONNECTED` | Device is not connected |
| `CAPABILITY_UNAVAILABLE` | Current Host lacks the requested capability |
| `RUNTIME_CLOSED` | Runtime has closed |
| `INTERNAL_ERROR` | Internal App or Studio error |

Use bounded retry for disconnection, busy, or rate-limit errors. Never retry in
an infinite loop. Stop old work after `STALE_RUNTIME`, fix parameter errors
instead of retrying them, and show a clear H5 message for user-visible failures.

## 10. Debug with Studio

For a static H5 directory containing `index.html`:

```sh
node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin
```

For Vite or Webpack, build first and load the final output directory:

```sh
npm run build

node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin/dist
```

After changing source, rebuild and select **Reload** in Studio. The browser-only
Studio loads static files and does not start Vite or provide HMR.

Studio simulates:

- the 600×350 green monochrome display;
- single-click, double-click, and long-press actions;
- head up, head down, turn left, and turn right;
- device connection and disconnection;
- `running`, `suspended`, `stopped`, and `failed` lifecycle states;
- Bridge request, response, and event logs;
- the 81,901-byte Channel 2/6/7 limit;
- raw LZ4 decoded-size validation; and
- the channel, payload size, and LZ4 decoded size of the most recent drawing.

An oversized request fails with `PAYLOAD_TOO_LARGE` and is not rendered. Studio
suggests a maximum tile height. Validate tiling in Studio before testing on
physical glasses.

Studio targets Bridge and device-canvas compatibility, not complete optical
simulation. Validate firmware fonts, visible brightness and contrast, lens
distortion, view distance and field of view, device refresh and transport
latency, gesture thresholds, and exact App/firmware compatibility on hardware.

## 11. Package and install an `.mmpkg`

`.mmpkg v1` is a ZIP container whose files are directly at the archive root:

```text
weather-1.0.0.mmpkg
├── manifest.json
├── index.html
└── assets/
    ├── index.js
    └── index.css
```

Do not add an outer `weather-1.0.0/` directory.

Build the H5 output, then run the packager:

```sh
npm run build

node /path/to/devkit/tools/build-mmpkg.mjs \
  /absolute/path/to/my-plugin/dist \
  /absolute/path/to/release/my-plugin-1.0.0.mmpkg
```

The input must be the final deployable output, not a project root containing
`src`, tests, project configuration, or `node_modules`. The output must use the
`.mmpkg` extension, must not be inside the input directory, and should include
the plugin name or ID and version.

The packager validates the manifest and entry, rejects symbolic links and
unsafe paths, calculates SHA-256 for payload files, writes `schemaVersion: 1`
and the complete `files` table, checks resource limits, and atomically produces
a deterministic ZIP.

Package limits:

```text
Maximum archive: 10 MB
Maximum extracted size: 30 MB
Maximum individual file: 10 MB
Maximum regular files: 500
```

Symbolic links, duplicate or absolute paths, path traversal, undeclared payload
files, and declared but missing files are forbidden. `signature.sig` is
reserved and excluded from `files`.

The App validates the extension, archive size, ZIP, manifest, paths, resource
limits, and every SHA-256 hash; extracts to a temporary directory; atomically
activates the installed version; and starts the plugin WebView. Plugins cannot
access other plugin directories. An installed plugin takes precedence over an
App-bundled demo with the same ID; uninstalling restores the bundled version.

Current local sideloading is Debug-only. Production release still requires
review, server signing, trusted App key verification, marketplace delivery,
updates and revocation, a permission UI, and request-level WebView network
isolation. Do not distribute unsigned local packages to normal Release users.

See [Final `.mmpkg` package](web-plugin/package-format.md) and
[LAN installation](web-plugin/lan-install.md) for complete details.

## 12. CSP and resource security

Use a strict Content Security Policy, for example:

```html
<meta
  http-equiv="Content-Security-Policy"
  content="
    default-src 'self';
    script-src 'self';
    style-src 'self' 'unsafe-inline';
    img-src 'self' data:;
    connect-src https://api.example.com;
    object-src 'none';
    base-uri 'none';
    frame-ancestors 'none';
  "
>
```

- Do not use `eval` or execute untrusted scripts dynamically.
- Do not load JavaScript from an unreviewed CDN.
- Package JavaScript, CSS, fonts, and images whenever possible.
- Allow network requests only to required HTTPS domains.
- Never hard-code tokens, passwords, or private keys in H5 files.
- Treat device-event and network-response strings as untrusted.
- Escape user input and remote content.
- Do not attempt to access capabilities the App did not authorize.

## 13. Migrate from ZIP to npm

The planned npm workflow is:

```sh
npm install @memomind/gm-plugin-web-sdk
npm install --save-dev @memomind/gm-plugin-studio
```

Replace the local import:

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
```

with:

```js
import { createGMPlugin } from '@memomind/gm-plugin-web-sdk';
```

Start Studio with:

```sh
npx gm-plugin-studio --plugin ./dist
```

Migration does not change `createGMPlugin()` usage, Bridge v1 methods or
events, the manifest, the `.mmpkg` format, or App installation and permission
validation.

## 14. Troubleshooting

### The page remains on "Connecting to App"

Confirm that the page was opened through Studio or the real App rather than
directly with `file://`. Verify that the SDK loaded, inspect the browser console
for 404, CSP, or module-resolution errors, and check the relative SDK import.

### Studio shows the page but buttons do nothing

Call `device.subscribeEvents()` for `button` or `imuGesture` and register the
corresponding `onButton()` or `onGesture()` listener.

### Nothing appears on the glasses

Phone HTML is not mirrored automatically. Call `gm.display.createPage()` and a
drawing method such as `gm.display.updateText()`. Check connection state and
make sure the drawing region is inside 600×350.

### Studio text differs slightly from hardware

Studio approximates fonts, wrapping, and green brightness with browser Canvas.
Validate firmware fonts, glyphs, optical brightness, and real refresh behavior
on physical glasses.

### The App rejects an `.mmpkg`

Check the extension, root-level `manifest.json`, entry file, Bridge version,
semantic version, permissions, resource limits, and hashes. Do not edit a
generated `.mmpkg` manually; rebuild it after every source change.

## 15. Pre-release checklist

### Project and SDK

- [ ] The final output contains `index.html` and the SDK bundle.
- [ ] All assets use package-relative paths.
- [ ] No development-computer absolute paths remain.
- [ ] Other APIs are called only after `gm.ready()` resolves.

### Manifest

- [ ] `id` is unique and uses reverse-domain form.
- [ ] `version` is a semantic version.
- [ ] `entry` points to an existing HTML file.
- [ ] `bridgeVersion` is `1.0`.
- [ ] Only required permissions are declared.

### Functionality

- [ ] The plugin page runs correctly in Studio.
- [ ] Device drawings remain in bounds and are not clipped.
- [ ] Click and head-motion events behave as expected.
- [ ] Disconnection does not trigger infinite retry.
- [ ] Suspend and resume logic works.
- [ ] Plugin state saves and restores correctly.

### Security and packaging

- [ ] CSP allows only required resources and network domains.
- [ ] No sensitive token, password, or private key is hard-coded.
- [ ] The DevKit packager produced the `.mmpkg`.
- [ ] No file was edited after packaging.
- [ ] The package remains within every resource limit.
- [ ] Installation and launch were verified in the target Debug App.

### Physical-device validation

- [ ] Fonts, wrapping, and layout were confirmed on physical glasses.
- [ ] Green brightness and contrast are readable.
- [ ] Image transport and refresh latency are acceptable.
- [ ] Rapid input does not damage the image or corrupt state.
- [ ] App, plugin, Bridge, and firmware versions were recorded.

## Related documentation

- [Web plugin documentation index](web-plugin/README.md)
- [Bridge v1 API overview](web-plugin/api-reference.md)
- [Debugging with Studio](web-plugin/studio.md)
- [Final `.mmpkg` package](web-plugin/package-format.md)
- [Compatibility and on-device limits](web-plugin/compatibility.md)
