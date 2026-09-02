# Memo Novel Reader Web plugin

This prototype imports a local TXT file in the phone WebView, normalizes it to
UTF-8, keeps the book and reading state in phone-side IndexedDB, and serves
bounded text windows to the paired `novel_reader.gmp`. The glasses plugin owns
font measurement, wrapping, automatic scrolling, page continuity, and device
input. It never persists novel data.

Version 0.1.11 temporarily bundles `晚明_柯山梦.txt` for device testing.
On first launch the Web plugin copies that UTF-8 text into its phone-side
IndexedDB bookshelf and opens it automatically, so the Android file chooser is
not required for this test build. Bridge callbacks are registered before the
large bundled text is decoded so a slow Android WebView cannot miss the Host's
initial bootstrap callback.

The phone synchronizes the exact title from the parsed directory with the
glasses. The frameless reading view shows that title at bottom left and the
reading progress at bottom right.

The phone controls and status messages use English. Imported novel text and
detected chapter titles remain in their original language.
The reader heading uses a dedicated title row followed by a separate
three-button action row so long names cannot collapse the mobile layout. The
current chapter appears below the preview, immediately above reading progress.

The phone page intentionally shows only a compact excerpt near the current
reading offset. The full text remains available to the glasses-side streaming
protocol and is not rendered as a long phone preview.

Reading progress is persisted at a throttled interval instead of every device
progress event, and library DOM updates occur only when persisted state changes.

The three automatic scrolling presets are 8, 16, and 24 pixels per second.
Saved settings from the earlier 4/8/12 profile are migrated once on open.

Supported input encodings are UTF-8, UTF-16 LE/BE, and GB18030. Automatic
detection prefers BOM information, then UTF-16 byte patterns, UTF-8, and
GB18030. A source TXT is limited to 20 MB.

Run with Browser Studio for phone-side import and storage checks:

```sh
node tools/run-browser-studio.mjs --plugin examples/novel-reader
```

Browser Studio does not run the paired `.gmp`. Use Desktop Studio and load both
this directory and the GlassSDK `novel_reader.gmp` to test text transfer,
device-side wrapping, scrolling, controls, and privacy cleanup.

The Android App WebView must implement `WebChromeClient.onShowFileChooser()`
and return the selected URI through its `ValueCallback<Array<Uri>>`. The iOS
App must allow document selection for `<input type="file">` in `WKWebView`.
The Host must also preserve IndexedDB for the installed plugin. These Host
behaviors require validation on a physical phone; plugin JavaScript cannot
read arbitrary phone files when the native file chooser is unavailable.
