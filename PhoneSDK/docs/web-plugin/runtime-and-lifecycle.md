# Runtime Model and Lifecycle

The same Web plugin code can run in two hosts:

- App WebView: the H5 SDK uses the `MemoPluginBridge` JavaScript channel.
- Desktop Studio and PhoneSDK Browser Studio: the H5 SDK uses the parent-frame
  `postMessage` transport.

The Host uses a session token and runtime generation to isolate old pages.
After a page reload, requests and events from the previous generation must not
enter the new runtime.

The current lifecycle states are `starting`, `running`, `suspended`, `stopped`,
and `failed`. A plugin must handle suspension, resumption, disconnection, and
runtime reconstruction instead of assuming that its page remains resident.
