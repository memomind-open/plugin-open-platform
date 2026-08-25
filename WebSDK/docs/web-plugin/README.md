# Web Plugin Development Documentation (Draft)

This directory contains the first public draft of the GM Web Plugin developer
documentation. The current contract version is Bridge v1.

## Recommended reading order

1. [Quick start](quick-start.md)
2. [DevKit ZIP guide](devkit-zip.md)
3. [Runtime model and lifecycle](runtime-and-lifecycle.md)
4. [API overview](api-reference.md)
5. [Debugging with Studio](studio.md)
6. [Final `.mmpkg` package](package-format.md)
7. [Install an `.mmpkg` over a LAN](lan-install.md)
8. [Compatibility and on-device limits](compatibility.md)

A Web plugin's HTML page runs in the phone App or Studio. Only content
submitted through `gm.display.*` is rendered on the glasses display.
