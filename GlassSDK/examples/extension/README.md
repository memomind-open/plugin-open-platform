# Extension discovery

Queries the published LZ4 extension table through `extension_get`, validates a
representative function pointer, and calls it. Use this pattern for post-1.0
optional modules without changing the frozen core Host table. The `lz4`
example demonstrates a complete compression and decompression round trip.

After changing this example, run `./gm-build` from the SDK root.
