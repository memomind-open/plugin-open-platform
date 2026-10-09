# Paired protocols and integration

First check whether standard `gm.display` and `gm.device` APIs can satisfy the requirement. If they can, pair with the existing [web_bridge](../../../../GlassSDK/examples/web_bridge/) without adding application channels or rewriting installation flows.

For a dedicated pair, first define a message table shared by both implementations: direction, channel, version, length, field types and byte order, response/ACK, retries, and state reset. Refer to [fighter-controller/protocol.js](../../../../PhoneSDK/examples/fighter-controller/protocol.js) and the [novel_reader README](../../../../GlassSDK/examples/novel_reader/README.md), adapting only the semantics the task needs.

## Manifests and pairing

- Give the new Web/Glass applications independent package identities. Both components may use the same application ID, while directory and version formats still follow their respective SDKs.
- Align Web `deviceRequirements` with Glass `provides.protocols`. Set `requiredPluginId` only when the application must use a specific GMP. For interchangeable display implementations, prefer protocol constraints with `preferredPluginId`.
- When retaining an existing protocol, preserve actual compatibility and its ID. For incompatible payloads, define a new protocol ID/version and update both requirements and providers. Do not claim compatibility in the manifest while changing only one side's encoding.
- Include the custom channels actually sent and received in Web `device.messaging.scope.channels`. List only subscribed event types in `device.events.scope.types`. Registering a listener does not grant permission, and a declaration does not establish runtime approval.
- Check matching rules in [PROTOCOL_COMPATIBILITY](../../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md). Keep a manifest beside a separately imported GMP; otherwise Studio may report unknown compatibility.

## Data and failure handling

Validate version, length, and ranges before decoding. Bridge/BT acceptance does not establish that the application executed a request. Where reliable state matters, define business ACKs, timeouts, duplicate handling, and bounded retries without busy-waiting in display callbacks.

Clear controller button state when touch is released, the window loses focus, or the connection drops. The GMP should detect expired control packets according to the protocol and restore neutral input instead of leaving a button permanently pressed. Show an offline state while disconnected and resynchronize necessary state after reconnection.

When drawing through standard Web Bridge APIs, do not manually implement the entire GM envelope. Send atomic frames serially according to begin/tile status ACKs. During telephone UI, continue receiving and acknowledging while suppressing drawing, then resend static content that needs restoration after the call.

## Minimal integration sequence

Verify each side's state machine and codec first. Then select the exact pair in Desktop Studio and verify one button action, one state change, and one response. After that works, add continuous input, larger payloads, disconnect/reconnect scenarios, and malformed messages. Browser Studio success does not prove that a custom GMP received data.

Finally, export the selected pair as a combined ZIP and verify it on a supported App/firmware combination. If no device is available, deliver the local packages and clearly identify unverified device behavior.
