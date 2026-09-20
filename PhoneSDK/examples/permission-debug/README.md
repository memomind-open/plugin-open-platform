> 2026-09-08: Controlled paired communication was restored: 10 permission categories and 32 Bridge methods. Custom paired H5/device plugins declare device.messaging and channels, then use gm.plugin.sendMessage/onMessage for bidirectional communication after authorization. Unauthorized, out-of-scope, and stale-runtime operations are rejected. Standard display/event channels still require their corresponding additional permissions. The lab tests only nine standard capabilities and has no arbitrary-message buttons. Messaging permission does not imply that the App isolates sensitive behavior inside device plugins; paired hardware acceptance testing remains pending.

# Plugin Capability Lab 0.2.8

This plugin exercises real features rather than merely displaying logs. The Host uses the existing Bridge 2.0 without upgrading WebView.
Refresh the phone plugin list in the permission development version of Desktop Studio and select "Plugin Capability Lab · Bridge 2.0 0.2.8".
Pair with GM Web Bridge for display tests. The lab does not provide general-purpose messaging.

All nine permissions are optional, allowing deny-all, partial-grant, and grant-all launches to be compared using real operations.
Capabilities unavailable from the Host still use the real interface and display the rejection reason. H5 does not call the computer microphone directly as a substitute for glasses capture.

| Permission | Available operations |
| --- | --- |
| storage | Save, read, and delete a note using one dedicated key |
| files.user-selected | Host file picker, file list, binary stream reads, text/image/audio previews, and confirmation before deleting a single file |
| display | Edit and send text to the glasses, send a GRAY_4 checkerboard, and close the page |
| device.info | Manually read device information and display its fields |
| device.events | Subscribe/unsubscribe, button-press count, head-motion indicator, and connection changes |
| audio.capture | Native glasses capture in recording mode, stop, capture state, and reception of Opus data and frame boundaries in H5 |
| audio.playback | Locally generated WAV test tone, delayed play(), and H5 playback of received glasses Opus recordings |
| network | Editable CORS-enabled URL, actual H5 GET, status/elapsed time/response body, timeout, and cancellation |
| location.foreground | Native one-shot/watch/cancel operations, coordinates, accuracy, sample time, and an offline track plot |

File previews are limited to 1 MiB and network response bodies to 64 KiB. Locations and files are not uploaded.
The test tone is a two-second, low-amplitude PCM WAV. Screen locking/page hiding preserves active audio playback and recording sessions but cancels network, location, and event subscriptions. Full cleanup occurs only when the page actually unloads.
Grant decisions are not saved, and H5 localStorage is not used as a substitute for the storage Bridge.

## Changes in 0.2.8

- Short recordings use the Web SDK's `openRecording()` convenience wrapper. The Host retains only the unified binary stream interface.

## Changes in 0.2.7

- The manifest name, page title, and main heading all use the English name `Plugin Capability Lab`.

## Fixes in 0.2.6

- Screen locking, `document.hidden`, `pagehide`, and Runtime `suspended` no longer proactively pause H5 audio or call `audio.stopCapture`.
- Screen locking still cancels network requests, location, and device-event subscriptions. `beforeunload` retains full cleanup.

## Fixes in 0.2.5

- Displayed recording frame count, Opus byte count, and capture duration; these metrics do not prove that intelligible speech was captured.
- Disabled duplicate-playback and test-tone buttons during native playback, restoring them on terminal states; displayed and logged complete playback errors.
- Corrected recognition of the capturing state. Page hiding stopped capture in this version; 0.2.6 changed that behavior.
- Used a CORS-enabled public test endpoint by default, without proxying requests or bypassing cross-origin restrictions.
- App log exports retained numeric recording statistics and hashed playback IDs, without saving audio content.

## Run directly

From the PhoneSDK directory:

```sh
npm run dev:permissions -- --port 4187
npm run pack:plugin -- examples/permission-debug dist/permission-debug-0.2.8.mmpkg
```

If this workspace already has a preview running on that port, refresh it.
Browser Studio's Reload preserves notes and files for the currently open workspace but requests authorization again each time. Closing or fully refreshing the Browser Studio page loses the in-memory simulator data. Desktop storage is managed by its Host.

## Suggested tests

1. Launch with all permissions denied: Bridge features should show NOT_GRANTED. Reload and select the permissions to test.
2. Write a note, clear the input, and read it back. Use Host Reload, authorize again, and read it again.
3. Select the included sample-note.txt. The preview should display its actual text, including the Chinese characters used to test Unicode file reading.
4. Send text and the checkerboard, then inspect the glasses or virtual display. The small plugin preview alone does not prove delivery.
5. Subscribe, then press the glasses button or use Studio Single click. The count should increase. Simulated head motion should move the indicator.
6. Play the test tone. Enter a test URL you are allowed to access and send a GET request.
7. Use the collapsible negative-test section for the rawImu scope and unknown network.request method checks.

## Testing boundaries

- Studio location comes from the Host simulator and is prominently labeled as simulated. On a real phone, the App's native result is displayed. The plugin does not bypass the Bridge with navigator.geolocation.
- macOS Desktop Studio uses the computer microphone to simulate glasses Opus capture and sends the data to H5 for playback. Real glasses recording still requires App + glasses validation.
- H5 playback and HTTP requests are real operations, but Studio does not provide native isolation. Playback or requests succeeding after permission denial means the Host did not block them; it is not an acceptance-test pass.
- GET failures may be caused by CORS or network errors and cannot automatically be classified as permission enforcement. The default is https://httpbin.org/get, a third-party public service that may be unavailable. Requests occur only after a click; the plugin does not contact external services automatically.
- The autoplay probe follows prior page interaction and cannot replace a real-device cold-start test without a user gesture.
- Existing restrictions on access to the App's full plugin Runtime still apply. This plugin does not bypass them.

Historical page testing on 2026-09-07 successfully exercised note storage/retrieval, text/checkerboard delivery, button counts, head-motion feedback, device information, test-tone play(), HTTP 200, and local file-stream previews.
Location clearly identified its simulated source, and recording returned CAPABILITY_UNAVAILABLE from the real interface. This historical testing does not establish that the current version has passed hardware acceptance testing.
