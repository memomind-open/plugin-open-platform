> This development branch uses Bridge 2.0. Follow the [permission debugging guide](permission-debug.md) for permission and API testing. Bridge 1.0, string-based permissions, and legacy audio methods in the older documentation below no longer apply.

# Web Plugin Development Documentation (Draft)

This directory contains the first public draft of the GM Web Plugin developer
documentation. The current contract version is Bridge 2.0; use the permission
and capability contracts for current declarations.

## Recommended reading order

1. [Quick start](quick-start.md)
2. [Complete developer guide](developer-guide.md)
3. [DevKit ZIP guide](devkit-zip.md)
4. [Runtime model and lifecycle](runtime-and-lifecycle.md)
5. [API overview](api-reference.md)
6. [Debugging with Desktop Studio and Browser Studio](studio.md)
7. [Final `.mmpkg` package](package-format.md)
8. [Install an `.mmpkg` over a LAN](lan-install.md)
9. [Compatibility and on-device limits](compatibility.md)

A Web plugin's HTML page runs in the phone App, prebuilt Desktop Studio, or
PhoneSDK Browser Studio. Only content submitted through `gm.display.*` is
rendered on the glasses display. Desktop Studio is the recommended debugger for
paired Web and `.gmp` development; Browser Studio is the lightweight Web-only
tool included in the DevKit ZIP.

## Detailed capability contracts

- [Paired-plugin application messages](application-messaging.md)
- [Foreground phone location](location.md)
- [Current permissions and capability boundaries](capability-contract.md)

## Native Bluetooth clients

For integrations outside the H5 Host, see the
[Bluetooth/BLE developer guide](../../../GlassSDK/docs/BLUETOOTH_DEVELOPER_GUIDE.md)
and [byte-by-byte wire examples](../../../GlassSDK/docs/WIRE_EXAMPLES.md).
These document HUD control, Opus capture, native HFP playback, and HOGP without
requiring the private App source. GMP executable delivery remains App-managed.
