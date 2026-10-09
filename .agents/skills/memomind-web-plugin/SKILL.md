---
name: memomind-web-plugin
description: Develop or fix MemoMind PhoneSDK H5 plugins, the JavaScript Bridge, permissions, lifecycle handling, rendering, and Browser Studio. Use for Web plugins and public SDK source, not the website Portal or native glasses ABI implementations.
---

# Web plugins and the Bridge

Start with the [PhoneSDK guide](../../../PhoneSDK/README.md) and [capability and permission contract](../../../PhoneSDK/docs/web-plugin/capability-contract.md). Use the [quick start](../../../PhoneSDK/docs/web-plugin/quick-start.md) for layout and run instructions only; its explicitly outdated permission/audio examples are not the current API contract. For a new application, start with the [application skill](../memomind-example-app/SKILL.md). Document paths are relative to this file; npm commands below run from `PhoneSDK/`.

## Choosing where to work

Keep application logic in `examples/<plugin>/` and use the closest example as a reference. Before changing a public API, read the [API reference](../../../PhoneSDK/docs/web-plugin/api-reference.md) and inspect implementations and tests in `packages/web-sdk` and `bridge-contract`.

Package responsibilities are described in the [package guide](../../../PhoneSDK/packages/README.md): web-sdk and the renderer use the contract, while studio-runtime uses the contract and renderer. Use current contract and source versions rather than legacy string permission arrays.

- Pages and rendering: read the API reference and display-control-lab; check GRAY_4, bitmap size, LZ4, and atomic-frame limits.
- Cross-platform messages: read [application messaging](../../../PhoneSDK/docs/web-plugin/application-messaging.md) and refer to SDK-based fighter-controller or life-desk. App-counter calls the App Bridge directly and is not a general host template. See [compatibility](../../../PhoneSDK/docs/web-plugin/compatibility.md) for simulator/device differences.
- Recording, files, location, or permissions: read the relevant API contract and [permission debugging](../../../PhoneSDK/docs/web-plugin/permission-debug.md); also read [location](../../../PhoneSDK/docs/web-plugin/location.md) for location tasks.
- Hosts and restarts: read [runtime and lifecycle](../../../PhoneSDK/docs/web-plugin/runtime-and-lifecycle.md) and [Studio](../../../PhoneSDK/docs/web-plugin/studio.md).

## Implementation rules

Use public `gm.*` APIs to access the Bridge. `device.messaging` is not a raw GATT or installer channel. Current manifests use permission objects; declare only the event types and message channels actually used. Runtime capabilities determine availability, and declarations do not grant authorization.

Handle required and optional permissions separately. Provide usable error or fallback paths for denial, revocation, unsupported capabilities, and disconnection. Do not treat cached authorization results as permanent grants.

Handle suspended/stopped/failed states and runtime reconstruction. Clean up subscriptions, timers, and asynchronous work. Responses and events from old sessions/generations must not enter the new page. App WebView and Studio use different transports while plugins keep the same public API.

After changing the SDK, check whether vendored copies in examples are affected. Use `npm run sync:example-sdk` when needed for the seven explicitly listed maintained examples, then review the generated changes. That command does not discover new projects. Refresh a new application individually with the application generator's `--refresh-sdk <project>` mode so it does not continue running an old SDK.

## Verification and delivery

Select existing Node tests for the affected code, such as `node --test packages/web-sdk/test/sdk.test.mjs`. For public contract or cross-package changes, run `npm run verify` (tests and workspace checks). When dependencies need installing, use the lockfile with `npm ci` without incidental upgrades.

From PhoneSDK, preview the new program with `node tools/run-browser-studio.mjs --plugin examples/<new-name>`. `npm run dev` always opens app-counter and `npm run dev:permissions` always opens permission-debug, so neither validates a newly created program. Browser Studio does not execute GMP files. Use prebuilt Desktop Studio for paired integration, and record physical-device permission, Bluetooth, and lifecycle checks separately.

Package a deployable H5 directory. Read [package format](../../../PhoneSDK/docs/web-plugin/package-format.md) and the delivery skill. Source trees, node_modules, or an old package are not a new deliverable.
