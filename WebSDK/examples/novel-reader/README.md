# Memo Novel Reader Web plugin

Version 0.3.0 imports user-selected TXT novels through the Host Bridge and
stores the selected source files in the App's private plugin directory. The
phone plugin normalizes text to UTF-8 and serves bounded text windows to the
paired `novel_reader.gmp`. The glasses plugin owns font measurement, wrapping,
automatic scrolling, page continuity, and device input. It never persists
novel data.

## Persistent library

The Web plugin declares `files.user-selected` and `storage` permissions and
uses the corresponding Bridge APIs:

- `files.pick` imports a user-selected TXT into App-managed private storage.
- `files.list` rebuilds the bookshelf after plugin or App restart.
- `files.openRead` opens a runtime-bound raw binary stream. File bytes bypass
  Bridge JSON and Base64, and the plugin never retains a resource URL or a
  temporary picker URI.
- `storage.get` and `storage.set` persist metadata, progress, bookmarks, font,
  speed, and playback state under a key derived from the stable `fileId`.
- `files.delete` removes the source file, then the plugin removes its matching
  key-value state.

The plugin does not impose a per-book size limit or a book-count limit. The
Host App owns the private file quota and currently advertises a 400 MiB total
limit. The page displays current Host-reported usage.

Versions through 0.1.11 stored books in WebView IndexedDB. Those records do not
contain the stable App `fileId` required by the new storage model, and a safe
automatic migration is not possible. After upgrading to 0.3.0, import each
previous book once. Its old IndexedDB progress and bookmarks cannot be restored.

## Reader behavior

The phone synchronizes the exact title from the parsed directory with the
glasses. The frameless reading view shows that title at bottom left and reading
progress at bottom right. The phone controls and status messages use English;
novel text and detected chapter titles remain in their original language.

The phone page shows only a compact excerpt near the current reading offset.
On first import the plugin scans the binary stream once to detect encoding and
build a persistent chapter/byte index without retaining the full novel. During
reading it opens only the source range needed for the next glasses text window
and keeps a small six-window memory cache. Reopening an indexed book does not
rescan or load the full file. Reading progress is persisted at a throttled
interval together with its source-window anchor.

The three automatic scrolling presets are 8, 16, and 24 pixels per second.
Saved settings from the earlier 4/8/12 profile are migrated once on open.

Supported input encodings are UTF-8, UTF-16 LE/BE, and GB18030. Automatic
detection prefers BOM information, then UTF-16 byte patterns, UTF-8, and
GB18030.

## Test and package

Browser Studio includes a local file-picker adapter for Bridge and UI checks.
Its in-memory private files survive plugin-frame reloads, but not a full Studio
page restart. Use an App Host implementing the `files.*` Bridge contract for
end-to-end phone import, App restart, and plugin-upgrade persistence checks:

```sh
node tools/run-browser-studio.mjs --plugin examples/novel-reader
```

Desktop Studio can load this directory together with the GlassSDK
`novel_reader.gmp` to test text transfer, device-side wrapping, scrolling,
controls, and privacy cleanup.

Build the Web plugin package from `WebSDK`:

```sh
node tools/build-mmpkg.mjs examples/novel-reader dist/memo-novel-reader-0.3.0.mmpkg
```
