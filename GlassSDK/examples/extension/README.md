# Extension discovery

Queries the published random and LZ4 extension tables through `extension_get`,
validates representative function pointers, and calls them. Use this pattern
for post-1.0 optional modules without changing the frozen core Host table. The
`lz4` example demonstrates a complete compression and decompression round trip.

After changing this example, run `./gm-build` from the SDK root.
