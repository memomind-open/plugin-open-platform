> 2026-09-10: Plugin Capability Lab 0.2.8 migrated short recordings to the Web SDK's `openRecording()` convenience wrapper. The Host retains only the unified binary stream interface. The naming changes from 0.2.7 remain in effect.
>
> 2026-09-10: Version 0.2.6 separated screen locking/background suspension from actual page unloading. `document.hidden`, `pagehide`, and Runtime `suspended` no longer proactively pause H5 audio or stop recording, allowing the Host's actual lock-screen behavior to be tested.

> 2026-09-08: Controlled paired communication was restored: 10 permission categories and 32 Bridge methods. Custom paired H5/device plugins declare device.messaging and channels, then use gm.plugin.sendMessage/onMessage for bidirectional communication after authorization. Unauthorized, out-of-scope, and stale-runtime operations are rejected. Standard display/event channels still require their corresponding additional permissions. The lab tests only nine standard capabilities and does not provide arbitrary-message buttons. Messaging permission does not imply that the App isolates sensitive behavior inside device plugins; paired hardware acceptance testing remains pending. This update and the restoration plan supersede the historical status below.

> Historical note (superseded by 0.2.3): Fix release **0.2.1** used `PhoneSDK/dist/permission-debug-0.2.1.mmpkg`. The SDK required `gm.audio.stopCapture(sessionId)` with the recording ID returned by openCapture. The lab implemented normal stop/exit cleanup and read captureState.result.recordingId. Reimport the new package before testing; 0.2.0 does not update automatically. rawImu/1001/unknown-method buttons are expected to be rejected. A CORS error from fetching Baidu does not prove network blocking; the test server must allow cross-origin access.
> Version 0.2.0 introduced real operations for ten permissions. See the [lab guide](../../examples/permission-debug/README.md) for usage and limitations. References to 0.1.0 below are historical.

# Bridge 2.0 Permission Debugging (Development Branch)

The PhoneSDK and Desktop Studio permission development branches were aligned with their respective latest `main` branches for this work. Debugging and acceptance testing require SDKs, plugin packages, and Desktop Studio installers rebuilt from this branch.

## Start debugging

Run from the PhoneSDK directory:

```sh
npm run dev:permissions
```

Open the address printed in the terminal (default: http://127.0.0.1:4173). The preview address used in the recorded session was http://127.0.0.1:4187.

All nine permissions in Plugin Capability Lab 0.2.8 are optional. Granting only storage should allow notes to be written, while ungranted location and recording operations should be rejected. Even after granting device.events, the negative rawImu probe should still be rejected as out of scope.
To test messaging permission, select a custom paired plugin such as Novel Reader, Fighter Controller, or Talking Pet and its matching device plugin. The lab has no messaging buttons. Authorization is requested on every launch.

## Desktop Studio

Run the simulator built from the source for this change, rather than an older version in Studio/prebuilt or /Applications.
Run in the plugin_studio repository, substituting your checkout paths:

```sh
GM_PHONE_SDK_ROOT=/absolute/path/plugin-open-platform/PhoneSDK \
GM_DEVICE_SDK_ROOT=/absolute/path/plugin-open-platform/GlassSDK \
npm run dev:debug
```

Select "Plugin Capability Lab · Bridge 2.0" in the phone plugin list and complete Host authorization. The dedicated example can test storage, location, and rejection paths without a glasses plugin. Display operations and real plugin messaging require a matching Glass plugin.
The revoke button stops the plugin and invalidates file-stream sessions. Request authorization again after a refresh or restart.

## Packaging

```sh
npm run sync:example-sdk
npm run pack:plugin -- examples/permission-debug dist/permission-debug-0.2.8.mmpkg
```

The package uses schemaVersion=2, permissionPolicyVersion=1, and bridgeVersion="2.0", with permissions declared as a strictly validated array of objects.
Example source manifests have been migrated and standard dist packages for the current version have been regenerated. Tic-Tac-Toe is a separate Vite project: rebuild in its own directory after source changes instead of using an old dist output.

## Capabilities and limitations

- Supported declarations are storage, files.user-selected, display, device.info, device.events, device.messaging, audio.capture, audio.playback, network, and location.foreground.
- The 32 methods use an exact allowlist; a matching prefix does not admit an unknown method. Events/channels are checked against scope, with additional display/device.events checks for system channels.
- The SDK removed audio.configure/startRecording/stopRecording/onFrames/onState. Use openCapture/stopCapture/onCaptureState. Legacy JSON Opus frames are not supported.
- Both SDK transports discard responses from old generations. Restarting does not allow an old response to complete a new request.
- Location is explicitly simulated and does not use computer geolocation. getCurrentPosition/watchPosition/clearWatch can be tested. Watches are canceled when the page is hidden; background location is not supported.
- Desktop uses the computer microphone to simulate glasses Opus capture and sends recording data to H5 through a binary port. The plugin page performs playback; there is no native playback by recordingId.
- network has no Bridge methods. The authorization panel records its declaration and selection, but this Browser/Desktop version **does not implement native network blocking or enforced audio muting**. Do not use it to accept network isolation or run untrusted plugins.
- Storage/files support functional debugging, but Studio does not implement the App's account, installationId, and packageDigest isolation. Host native commands are not a production sandbox either. Validate package updates and persisted authorization identity in the App.
- The App's Governed Runtime checks declarations and grants. The App container restricts ordinary HTTP requests for network isolation without upgrading or modifying the WebView library; this does not cover WebRTC.

## Rule sources and validation

App sources: `docs/plan/plugin_permission_governance/api/permission_contract.md` / `permission_vectors.json`, revision p7-handoff-r1.
PhoneSDK's `packages/bridge-contract/src/permission-policy.js` and Desktop's `desktop/ui/src/permission-policy.js` contain the same rule snapshot and each run the App test vectors. Keep both files and the vectors synchronized when updating them.
Rust package parsing independently runs the same declaration vectors and preserves required, reason, and scope instead of extracting only name.

```sh
# PhoneSDK
npm test
npm run check
# plugin_studio
npm test
cargo test --offline --manifest-path desktop/src-tauri/Cargo.toml
```

Manual browser regression checks: required-permission denial, minimal grants, successful storage, NOT_GRANTED, UNDECLARED, METHOD_NOT_FOUND, event/channel OUT_OF_SCOPE, simulated location watch/stop, and revoke/reauthorize. File selection, glasses display, real audio, and native network isolation on all three platforms still require separate hardware testing.

## Historical validation record (2026-09-05, with later package updates)

- PhoneSDK: 133/133 Node tests passed; workspace checks and git diff --check passed.
- Desktop: 118/118 UI tests, 6/6 tool tests, and 27/27 Rust tests passed; the macOS Universal 2 DMG build succeeded.
- Browser Studio: the actual page exercised required-permission denial, minimal grants, successful calls, three permission-rejection categories, location events/stop, and revoke/reauthorize.
- macOS Desktop: the newly built window discovered and loaded the permission lab, completed the Bridge 2.0 handshake, wrote storage, and returned OUT_OF_SCOPE and simulated location results.
- Recorded validation package: PhoneSDK/dist/permission-debug-0.2.8.mmpkg, SHA-256 `f88741fe13852b2533f7dd9aa0a982371c32ab32ca237603a3e7b25b19e8d420`.
- Historical 0.2.5 package SHA-256: `8815cfafdb2353f2e19592da66169dd10ddcb3b8b87ccedd68746c521a1e7ba3`. It proactively pauses playback and stops recording when the page is hidden, so it must not be used for lock-screen acceptance testing.
- WebView/Tauri dependencies were not upgraded. Business example packages for the current version were regenerated with Bridge 2.0 permission declarations.
- Not verified in this record: real phones/glasses, system location permissions, native network/audio isolation, and manual interaction with the actual file picker. The standalone Tic-Tac-Toe project was not built or tested.
