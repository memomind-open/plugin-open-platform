# Extension discovery

Queries the published random and LZ4 extension tables through `extension_get`,
validates representative function pointers, and calls them. Use this pattern
for post-1.0 optional modules without changing the frozen core Host table.
