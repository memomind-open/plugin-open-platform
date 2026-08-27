# SDK packages

`packages/` contains reusable runtime modules. These packages are not runnable
plugins and must not contain product-specific examples or generated bundles.

| Package | Responsibility | Direct internal dependencies |
| --- | --- | --- |
| `bridge-contract` | Bridge v1 methods, events, errors, permissions, and device limits | None |
| `web-sdk` | Public `gm.*` API and App/Studio transports used by Web plugins | `bridge-contract` |
| `device-renderer` | GRAY_4 framebuffer validation, LZ4 decoding, and Canvas presentation | `bridge-contract` |
| `studio-runtime` | Browser Studio host simulation, storage, lifecycle, and device events | `bridge-contract`, `device-renderer` |

The intended dependency direction is:

```text
bridge-contract ──► web-sdk
        │
        └─────────► device-renderer ──► studio-runtime
```

Run all package tests from the WebSDK root with `npm test`.
