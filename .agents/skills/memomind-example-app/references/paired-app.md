# Paired protocols and integration

First check whether standard `gm.display` and `gm.device` APIs can satisfy the requirement. If they can, pair with the existing [web_bridge](../../../../GlassSDK/examples/web_bridge/) without adding application channels or rewriting installation flows.

For a dedicated pair, first define a message table shared by both implementations: direction, channel, version, length, field types and byte order, response/ACK, retries, and state reset. Refer to [fighter-controller/protocol.js](../../../../PhoneSDK/examples/fighter-controller/protocol.js) and the [novel_reader README](../../../../GlassSDK/examples/novel_reader/README.md), adapting only the semantics the task needs.

## Delivery mode and the missing-companion check

Do not infer architecture from which selector happens to be populated in Studio. A standalone Glass application can run with the phone component disabled. A Web application using gm.scene can reuse the existing Web Bridge. A dedicated custom protocol requires a real provider on the glasses and a matching consumer/sender on the phone.

Before declaring a paired task complete, locate both source directories and build both deliverables. Merely adding `provides.protocols` does not implement a handler, and changing the package ID or setting `requiredPluginId` does not add missing protocol support. When adapting a one-sided implementation, implement the missing companion within the requested application scope or clearly identify the unresolved dependency.

## Manifests and pairing

- Give the new Web/Glass applications independent package identities. Both components may use the same application ID, while directory and version formats still follow their respective SDKs.
- Align Web `deviceRequirements` with Glass `provides.protocols`. Set `requiredPluginId` only when the application must use a specific GMP. For interchangeable display implementations, prefer protocol constraints with `preferredPluginId`.
- When retaining an existing protocol, preserve actual compatibility and its ID. For incompatible payloads, define a new protocol ID/version and update both requirements and providers. Do not claim compatibility in the manifest while changing only one side's encoding.
- Include the custom channels actually sent and received in Web `device.messaging.scope.channels`. List only subscribed event types in `device.events.scope.types`. Registering a listener does not grant permission, and a declaration does not establish runtime approval.
- Check matching rules in [PROTOCOL_COMPATIBILITY](../../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md). Keep a manifest beside a separately imported GMP; otherwise Studio may report unknown compatibility.

Audit Bridge calls against permissions as well as channel declarations. For example, `gm.device.getInfo()` needs `device.info`; subscribing to events also needs the corresponding event scopes. Remove unused starter permissions such as `display` when a custom GMP owns drawing. A permission marked optional requires a usable denied/unavailable path; it must not be treated as guaranteed during startup.

## Pairing preflight

From the repository root, check the actual package and the manifest belonging to the GMP you intend to select:

```sh
python3 .agents/skills/memomind-example-app/scripts/check_pair.py \
  --web PhoneSDK/dist/<app>.mmpkg \
  --glass GlassSDK/examples/<app>/manifest.json \
  --gmp GlassSDK/build-host/.build/<app>/<app>.gmp
```

Use the actual filename, including a version suffix when applicable. `--web` also accepts a source directory/manifest during development. For delivery, pass the built MMPKG to catch stale packaged requirements. `--gmp` checks file existence and header name/version/ABI against the Glass manifest. It does not prove source freshness, full GMP validity, handler implementation, permission coverage, or the current Studio selection. Runtime validation remains necessary.

The checker exits unsuccessfully for missing/old protocols, a required plugin mismatch, or mismatched GMP identity metadata. A different `preferredPluginId` alone is advisory when all required protocols are provided. Rebuild after changes; do not delete requirements or falsely advertise protocols to make the check pass.

Checker regression tests: `python3 .agents/skills/memomind-example-app/scripts/test_check_pair.py`.

## Diagnosing a selected pair

For example, a Web package requiring `gm.puppy-pet` and sending channel `19793` cannot communicate with the default `GM Web Bridge`, which implements gm.scene rather than that custom pet protocol. "Missing protocol gm.puppy-pet" describes a manifest mismatch; "Glass plugin did not handle channel 19793" means the selected running plugin did not consume the payload. Neither message establishes that a companion source directory is absent.

1. Read the actual phone MMPKG manifest and the Glass manifest corresponding to the selected GMP. Record both IDs, versions, paths, required protocols, and provided protocols.
2. If the correct companion source exists but its GMP is absent, build that specific example and confirm the expected output. If the binary exists, check discovery/layout and the selected path rather than generating another companion blindly.
3. Import the repository workspace, refresh both catalogs, and explicitly select the application's Web component and its matching Glass component. Studio preserves a previous explicit selection for diagnostics; Refresh may leave Default Web Bridge selected.
4. For an imported standalone GMP, keep its matching manifest available beside it. Unknown compatibility caused by missing metadata is distinct from a known missing protocol.
5. Confirm the incompatible warning is gone and execute one real action through the selected pair. Re-export the combined ZIP/QR after changing the selection; the old export may still contain the wrong GMP.

## Data and failure handling

Validate version, length, and ranges before decoding. Bridge/BT acceptance does not establish that the application executed a request. Where reliable state matters, define business ACKs, timeouts, duplicate handling, and bounded retries without busy-waiting in display callbacks.

Clear controller button state when touch is released, the window loses focus, or the connection drops. The GMP should detect expired control packets according to the protocol and restore neutral input instead of leaving a button permanently pressed. Show an offline state while disconnected and resynchronize necessary state after reconnection.

When drawing through standard Web Bridge APIs, do not manually implement the entire GM envelope. Send atomic frames serially according to begin/tile status ACKs. During telephone UI, continue receiving and acknowledging while suppressing drawing, then resend static content that needs restoration after the call.

## Minimal integration sequence

Verify each side's state machine and codec first. Then select the exact pair in Desktop Studio and verify one button action, one state change, and one response. After that works, add continuous input, larger payloads, disconnect/reconnect scenarios, and malformed messages. Browser Studio success does not prove that a custom GMP received data.

Finally, export the selected pair as a combined ZIP and verify it on a supported App/firmware combination. If no device is available, deliver the local packages and clearly identify unverified device behavior.
