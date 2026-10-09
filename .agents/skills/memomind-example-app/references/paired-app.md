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

Use the actual filename, including a version suffix when applicable. `--web` also accepts a source directory/manifest during development. For delivery, pass the built MMPKG to catch stale packaged requirements. `--gmp` checks file existence and header name/version/ABI against the Glass manifest. It does not prove source freshness, full GMP validity, handler implementation, permission coverage, Studio discovery, automatic selection, or the current Studio selection. Runtime validation remains necessary.

The checker exits unsuccessfully for missing/old protocols, a required plugin mismatch, or mismatched GMP identity metadata. A different `preferredPluginId` alone is advisory when all required protocols are provided. Rebuild after changes; do not delete requirements or falsely advertise protocols to make the check pass.

Checker regression tests: `python3 .agents/skills/memomind-example-app/scripts/test_check_pair.py`.

## Diagnosing a selected pair

For example, a Web package requiring `gm.puppy-pet` and sending channel `19793` cannot communicate with the default `GM Web Bridge`, which implements gm.scene rather than that custom pet protocol. "Missing protocol gm.puppy-pet" describes a manifest mismatch; "Glass plugin did not handle channel 19793" means the selected running plugin did not consume the payload. Neither message establishes that a companion source directory is absent.

1. If the expected glasses application is missing from the catalog, diagnose discovery using the sequence below. Do not tell the developer to select a nonexistent option or create another companion before checking existing files.
2. If it is listed, read the actual phone MMPKG manifest and the Glass manifest corresponding to the selected GMP. Record both IDs, versions, paths, required protocols, and provided protocols. Then test automatic matching; use explicit selection only to isolate a remaining selection problem.
3. For an imported standalone GMP, keep its matching manifest beside it. Unknown compatibility caused by missing metadata is distinct from a known missing protocol.
4. Confirm the incompatible warning is gone and execute one real action through the selected pair. Re-export the combined ZIP/QR after changing the selection; the old export may still contain the wrong GMP.

## Discovery and automatic matching

Treat source creation, building, Studio discovery, automatic matching, and runtime messaging as separate acceptance steps. A preferred ID only ranks discovered providers that satisfy the requirements; it cannot build or discover a missing package. Sharing a package name or ID is not enough.

1. **Build into the workspace catalog layout.** From the repository root, use `python3 GlassSDK/build.py build --example <relative-path>` for `GlassSDK/examples/<relative-path>`. Confirm both that source directory's manifest and `GlassSDK/build-host/.build/<relative-path>/<basename>.gmp` exist. In contrast, `--project <directory>` defaults to `<directory>/.build/<basename>/<basename>.gmp`; that nested project-local output is not a workspace catalog location. Use Import package plus a neighboring manifest for standalone output. A build listing reports expected paths, not successful builds or Studio discovery.
2. **Verify files on the machine running Studio.** Import the exact repository root containing both SDKs and refresh the glasses catalog. On a separate Windows machine, check that machine's local or network-share paths, not only files visible to the AI on Linux. Compare displayed names/versions and selected paths with the current manifest. Older displayed versions suggest stale discovery or a different copy; they do not prove which cause applies. Check discovery errors in the runtime log. Scan depth is release-dependent; prefer a direct SDK/examples child for new projects.
3. **Isolate discovery when an option is absent.** Import the exact built GMP through Import package, with its matching manifest available. If that succeeds but workspace refresh still omits it, investigate the workspace path, layout, access, and Studio release. If import fails, report the package/load error. Do not remove protocol requirements to bypass either failure. Keep sources, GMP, and review attachments from the same build when transferring to another machine.
4. **Exercise automatic matching.** Once the candidate is visible with readable protocol metadata, choose the phone application and verify the matching glasses name, ID, and path are selected automatically with compatible/recommended status. Refresh can preserve an explicit glasses choice; switching the phone selection can trigger matching again. Record the actual Studio release and observed behavior rather than assuming private source changes are present in the distributed executable. If only manual selection works, report automatic matching as unresolved.
5. **Exercise the pair.** Send one real application action, verify the glasses display/state and any expected reply, then export the combined ZIP/QR. Record discovered, automatically selected, and runtime-verified results separately. If Desktop Studio is unavailable, mark those checks pending and give concrete paths and commands; a local metadata pass does not complete them.

For example, on Windows PowerShell, run these read-only checks from the imported repository root (replace `my-app`):

```powershell
Get-Location
Get-Content .\GlassSDK\examples\my-app\manifest.json
Get-Item .\GlassSDK\build-host\.build\my-app\my-app.gmp | Select-Object FullName, Length, LastWriteTime
```

## Data and failure handling

Validate version, length, and ranges before decoding. Bridge/BT acceptance does not establish that the application executed a request. Where reliable state matters, define business ACKs, timeouts, duplicate handling, and bounded retries without busy-waiting in display callbacks.

Clear controller button state when touch is released, the window loses focus, or the connection drops. The GMP should detect expired control packets according to the protocol and restore neutral input instead of leaving a button permanently pressed. Show an offline state while disconnected and resynchronize necessary state after reconnection.

When drawing through standard Web Bridge APIs, do not manually implement the entire GM envelope. Send atomic frames serially according to begin/tile status ACKs. During telephone UI, continue receiving and acknowledging while suppressing drawing, then resend static content that needs restoration after the call.

## Minimal integration sequence

Verify each side's state machine and codec first. Then select the exact pair in Desktop Studio and verify one button action, one state change, and one response. After that works, add continuous input, larger payloads, disconnect/reconnect scenarios, and malformed messages. Browser Studio success does not prove that a custom GMP received data.

Finally, export the selected pair as a combined ZIP and verify it on a supported App/firmware combination. If no device is available, deliver the local packages and clearly identify unverified device behavior.
