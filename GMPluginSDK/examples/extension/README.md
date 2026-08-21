# Extension discovery

Defines a small optional extension table, queries it through `extension_get`,
validates the returned function pointer, and calls it. Use this pattern for
post-1.0 optional modules without changing the frozen core Host table.

After changing this example, run `./gm-build` from the SDK root.
