# Pixel Image Viewer

Select local images on the phone, keep them in numbered slots, and send the selected image to the glasses through the App's Bluetooth messaging bridge. All slots share one green preview. The glasses retain only the currently received frame. Both plugins are named **Pixel Image Viewer**.

## Usage

1. Import the repository workspace into Desktop Studio and refresh both plugin lists.
2. Select **Pixel Image Viewer** on the phone side and verify that Studio selects the matching glasses plugin with a compatible protocol.
3. Run the pair or export it to the App. Grant the declared file, storage, device information, connection event and messaging permissions.
4. Select a numbered slot and tap **Choose image**. Use another slot for each additional image. The trailing **+** adds and selects an empty slot.
5. Select a loaded slot, check its preview, and tap **Send to glasses**. Reaching 100% still requires the glasses to validate the complete frame and acknowledge FrameBuffer submission.
6. After cancellation, suspension or disconnection, resend the complete image in a new transfer session. A frame received during a call overlay is displayed after that overlay closes.

The phone starts with 10 slots. Loaded slots have a dot; switching slots only changes the preview and never sends automatically. Each loaded slot caches its preview, 105,000-byte GRAY4 frame and LZ4 payload. Switching or resending does not reread, decode or compress the file.

The **Image alignment** selector applies to the selected slot. Changing it immediately rebuilds the preview, GRAY4 frame and compressed payload; tap **Send to glasses** to display that placement on the glasses. Each slot remembers its alignment across restarts, including empty slots. A failed rebuild or save preserves the previous setting and frame. Changing alignment rereads the original file, while ordinary slot switching and sending still use the cache.

Slot count and file references persist in private plugin storage. On reopening, images are restored sequentially to avoid decoding several large files at once; slot 1 is selected initially. The older single-image setting is restored into slot 1. Full-resolution decoded images are released after preparing the fixed-size frame.

Replacing an image affects only its selected slot. Cancelling or failing to save keeps the previous image; failing to save a new slot leaves the count unchanged. Slot changes are disabled during initialization, file loading and sending. An old private file is deleted only when no slot references it. Original files on the phone are never modified.

## Pixel rules

- Output is exactly **600 × 350**. One decoded source pixel maps to one output pixel; there is no scaling or interpolation.
- Handle width and height independently: align smaller dimensions on black using the selected position, and crop larger dimensions starting at coordinate 0. The nine positions cover top/middle/bottom and left/center/right; center is the default.
- With the default center alignment, a 100×50 image starts at `(250,150)`; a 900×500 image contributes its top-left 600×350 pixels; an 800×100 image contributes its leftmost 600 columns at vertical offset 125.
- When centering, if padding is odd, the extra pixel goes on the right or bottom.
- Composite transparency on black, then quantize luminance to GRAY4 with 16 levels. Even x coordinates occupy the high nibble. Each row is 300 bytes; the entire frame is 105,000 bytes.
- Do not add text, watermarks or other content to the transmitted image. The preview renders the transmitted intensity as `(0, gray × 17, 0)` without converting that green preview back to grayscale.
- Use native image decoding and orientation metadata without resize parameters. PNG, JPEG, BMP and WebP support depends on the phone's decoder. Invalid, unsupported or excessively large files report an error rather than being silently scaled down. Use lossless PNG when inspecting individual pixels; JPEG itself is lossy.

The canvas backing store remains 600×350. Its CSS dimensions account for `devicePixelRatio × visualViewport.scale`: at DPR 3, it occupies 200×116⅔ CSS pixels, corresponding to 600×350 physical pixels. If the container is physically narrower than 600 pixels, horizontal scrolling preserves the pixel mapping.

## Interface and language

The plugin UI is English on every system locale. This includes headings, buttons, accessible labels, progress, help and plugin-defined errors. Filenames, image contents and unknown host diagnostics are preserved. The native file picker follows the system's own language.

Version 0.1.7 selected text using the WebView's `navigator.language`; Desktop Studio's separate Language setting was not propagated to the phone plugin. Version 0.1.8 made the plugin consistently English without requiring an APK, Studio or glasses update.

The compact interface places the title, version and connection state on one line. Slot buttons occupy a bounded, scrollable panel, with the **+** button as its final cell. **Choose image** / **Replace image**, **Send to glasses** and the alignment selector share one row above the preview. The selector has an accessible name without a visible label.

The existing filename row has a fixed height of 30px. It shows the filename when idle and reuses that space for loading, transfer, completion or error status. While sending, the percentage/encoding label, progress bar and **Cancel** stay on one line; the bar fills all remaining space. These state changes do not move the slots, action buttons or preview. Long messages do not wrap; the full text is available in the title attribute, and errors can scroll horizontally within the row.

**Cancel** and the progress bar appear only during transfer. **Check connection** appears when recovery is needed. Image details and display help are collapsed by default. Empty slots clear the preview to black and disable sending.

## Transfer and recovery

### GRAY4 and LZ4

The phone prepares the entire GRAY4 frame and caches a raw LZ4 block when loading an image. On send, HELLO queries the receiver's decompression capability. Use LZ4 only when supported and smaller than the original; otherwise send raw GRAY4 and report the fallback reason. Compression does not change padding, cropping or pixel values.

The glasses receive the complete payload, decompress it through the Host's `GM_PLUGIN_EXTENSION_LZ4.decompress_safe`, validate the decoded length and FNV-1a hash, then copy the frame into FrameBuffer. They do not decode JPEG/PNG or recalculate pixels. The protocol carries the lengths needed for the [raw LZ4 block format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md).

Image details expose the raw and compressed sizes. Completion reports the encoding, transmitted size and elapsed time from BEGIN through the COMMIT acknowledgement; this excludes precomputation and the HELLO exchange. Noise or photographs may not compress well. The sender never selects a compressed payload larger than the raw frame.

Glasses version 2 supports protocol 1.1 and also accepts older raw transfers. Peak requested heap for LZ4 is `105000 + compressed_bytes`, less than 210,000 bytes before allocator overhead. Compressed input is freed after decoding; only the decoded frame remains. Allocation failure is reported and does not present a partial frame. See the [native receiver documentation](../../../GlassSDK/examples/image_viewer/README.md) for packet fields and lifecycle behavior.

### Connection checks

The old 0.1.3 reconnect action called `location.reload()`, which could tear down GeckoView extension ports and crash the Android App. ADB captured `NullPointerException: NativeException NullHandle() [T = mozilla::widget::EventDispatcher]` through `WebExtension.Port.disconnectFromExtension → disconnected → EventDispatcher.shutdownNative`; the App process exited.

Starting with 0.1.4, **Check connection** reuses the current page and SDK, queries device state, and subscribes only if the connection subscription is missing. It preserves files, previews and prepared payloads. Initialization retries and browser back/forward cache restoration also reuse the page. Actual page teardown closes the SDK synchronously; host session cleanup releases subscriptions without an asynchronous unsubscribe during unloading.

This removes the plugin's reload trigger; it does not modify GeckoView. The action checks an existing connection and does not replace Bluetooth pairing/connection in the App's device screen.

### File selection

Use the same host-managed `gm.files.pick` / `openRead` path as Novel Reader. The host authorizes the actual API calls; do not disable file selection because `runtime.getCapabilities().methods` is incomplete. Device initialization failure still allows local image selection and preview.

An expired read ticket with exactly `INTERNAL_ERROR` and `Host file stream failed: HTTP 404` is retried after 100ms and 300ms, obtaining a fresh stream each time. Other file or permission errors are not retried. The implementation also handles picker suspension/resume, detects image MIME from file signatures, and falls back from ImageBitmap to the ordinary image decoder.

## Packages and pairing

Paths below are relative to the repository root.

| Component | Identity / version | Package |
| --- | --- | --- |
| Phone | Pixel Image Viewer / `com.memomind.example.image-viewer`, 0.1.15 | `PhoneSDK/dist/image-viewer-0.1.15.mmpkg` |
| Glasses | Pixel Image Viewer / `com.memomind.example.image-viewer`, 2 | `GlassSDK/build-host/.build/image_viewer/image_viewer.gmp` |
| Protocol | `gm.pixel-image-viewer`, 1.1 | Request 18774 (`0x4956`), acknowledgement 18775 (`0x4957`) |

Keep the GMP with its matching `.review.json` and `.review-source.enc`. When manually importing the GMP on another machine, also provide `GlassSDK/examples/image_viewer/manifest.json` for pairing metadata. Do not select the default Web Bridge: it does not implement this plugin's image protocol.

## Build, test and preview

From the repository root:

```sh
python3 GlassSDK/build.py build --example image_viewer
node PhoneSDK/tools/build-mmpkg.mjs PhoneSDK/examples/image-viewer PhoneSDK/dist/image-viewer-0.1.15.mmpkg
node --test PhoneSDK/tools/test/image-viewer.test.mjs PhoneSDK/tools/test/image-viewer-i18n.test.mjs PhoneSDK/tools/test/image-viewer-alignment.test.mjs
python3 -m unittest discover -s GlassSDK/tests -p test_image_viewer_native.py
python3 -m unittest discover -s GlassSDK/tests -p test_system_native.py
python3 .agents/skills/memomind-example-app/scripts/check_pair.py --delivery --web PhoneSDK/dist/image-viewer-0.1.15.mmpkg --glass GlassSDK/examples/image_viewer/manifest.json --gmp GlassSDK/build-host/.build/image_viewer/image_viewer.gmp
node PhoneSDK/tools/run-browser-studio.mjs --plugin examples/image-viewer
```

After exporting a combined ZIP, rerun the pairing check with `--bundle PhoneSDK/dist/pixel-image-viewer-0.1.15-paired.zip` to verify its actual package bytes and review hashes.

Ordinary Browser Studio validates file selection and UI but does not execute GMP code. Sending without a compatible receiver will time out; this does not establish physical glasses behavior.

The optional integration script requires a native C compiler, reference liblz4, Playwright and Chromium:

```sh
node PhoneSDK/tools/verify-image-viewer-browser.mjs
```

The script connects public Browser Studio messaging to the actual C receiver compiled with a simulated host and reference LZ4. It uses localhost port 4187 and does not modify SDK or Studio implementations. Set `PLAYWRIGHT_MODULE` and `CHROMIUM_EXECUTABLE` to existing installations if they are not discoverable. Native tests link with `-llz4` by default; systems with only the Linux runtime library can use `LZ4_LINK_FLAGS=-l:liblz4.so.1`.

`IMAGE_VIEWER_LOCALE` controls the test browser's locale (default `zh-CN`). The plugin remains English when the locale changes, including existing status/error messages, while keeping its document, image caches and subscriptions.

The integration creates two PNGs, loads them into slots 1 and 2, and checks 420,000 green preview pixels and every final FrameBuffer byte. It covers four LZ4 transfers, cached switching without rereads or automatic sends, 40 slots and restart restoration, empty previews, failed saves, file-ticket recovery, decoder fallback, initialization retry, connection checks without navigation or duplicate subscriptions, picker cancellation, suspension and permission revocation. At 320/390px widths it checks that sending, completion and errors preserve the positions of slots, controls and preview. Artifact paths are printed on completion.

The repetitive test patterns compress from 105,000 bytes to 620 bytes (100×50 centered image) and 556 bytes (900×500 cropped image), reducing 26 raw chunks to one. These are test-pattern sizes, not expected photograph sizes or measured physical Bluetooth throughput.

## Verification boundaries

Completed local checks include 23 JS tests, native C receive/call/lifecycle tests, browser-to-C pixel integration, and package/pair checks. Native tests exercise multiple compressed chunks, malformed sizes/blocks, hash failures, allocation failure, duplicate acknowledgements, disconnection, suspension, timeout and call restoration. Address/undefined-behavior checks passed in the development environment; LeakSanitizer was unavailable, so allocation-release assertions provide separate coverage.

The browser/C harness is not Desktop Studio's RISC-V runtime or a physical Bluetooth link. Final acceptance still requires Desktop Studio discovery/automatic pairing, GMP execution and combined ZIP/QR installation, plus Android/iOS file selection, large-image decoding, permission prompts, Bluetooth throughput/recovery, glasses heap allocation, call overlays and physical pixel mapping. Older firmware without the system-event extension cannot notify the plugin when a call overlay closes; resend manually when needed.

## Source map

- `image.js`: decoding, original-pixel placement, black padding and GRAY4 conversion.
- `i18n.js`: English messages and structured errors.
- `plugin.js`: slots, physical-pixel preview, persistence, controls and lifecycle.
- `lz4.js`: raw-block encoder adapted from the life-desk example.
- `protocol.js`: negotiation, packets, acknowledgements, retries and transfer statistics.
- `GlassSDK/examples/image_viewer/image_viewer.c`: receive validation, FrameBuffer submission and call restoration.

## Release history

- **0.1.15:** Place image selection, sending and alignment in one compact row without a visible alignment label.
- **0.1.14:** Add nine persistent per-slot image alignments to both preview and transmitted pixels.
- **0.1.13:** Fill the available status-row width with the progress bar.
- **0.1.12:** Reuse the filename row for fixed-height single-line transfer/status controls.
- **0.1.11:** Put connection state on the title/version row.
- **0.1.10:** Show connection recovery and cancellation controls only when needed.
- **0.1.9:** Remove redundant slot/preview headings and persistent instructions.
- **0.1.8:** Use a consistently English interface.
- **0.1.7:** Introduce locale-based messages, superseded by 0.1.8.
- **0.1.6:** Compact layout with bounded slot scrolling and actions above the preview.
- **0.1.5:** Add persistent, expandable image slots and cached switching.
- **0.1.4:** Check connections without reloading the WebView.
- **0.1.3:** Add cached GRAY4/LZ4 transfer and the version 2 glasses receiver.
- **0.1.2:** Share one green preview with physical-pixel sizing.
- **0.1.1:** Use host-authorized file calls and recover expired read tickets.
