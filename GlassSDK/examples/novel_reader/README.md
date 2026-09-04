# Novel Reader

This glasses plugin receives bounded UTF-8 windows from the paired Web plugin,
wraps them with the firmware font, and performs continuous vertical scrolling
locally. EPUB JPEG/PNG illustrations are represented by markers in that logical
text stream. When a marker becomes the current page, the glasses request a
600 x 350 GRAY_4 frame and commit its acknowledged row tiles atomically through
the Host framebuffer. Novel text and images are never written to persistent
storage. Only the text pages, one incoming text window, and the currently
presented Host framebuffer exist in volatile memory, and plugin-owned buffers
are cleared on disconnect, suspend, stop, or exit.

The reading view has no title or border. Its only status line shows the exact
directory title at bottom left and reading progress at bottom right.

Automatic scrolling batches display movement to 10 frames per second and only
updates the progress label when its percentage changes, reducing long-running
CPU load and repeated label allocations without changing reading speed.
The bottom-aligned reading viewport displays five scrolling text lines at a
time, directly above the chapter and progress status line.
The implementation uses the Host libc extension for memory operations and
formatting, avoiding private libc replacements and redundant window clearing.

Controls:

- Up / scroll up: move up one rendered line.
- Down / scroll down: move down one rendered line.
- Page up / page down: move one rendered page.
- Left / right: request the previous / next chapter from the phone.
- Primary single click: pause or resume automatic scrolling.
- Primary double click: save a bookmark on the phone.
- Primary long press: show the exit countdown.
- Release before the countdown completes: cancel exit.
- Back / Home: exit immediately.

Head gestures are intentionally not used, so normal head movement cannot
start or complete the exit countdown.

Pause, resume, and successful bookmark actions show a brief English status
message at the top center of the glasses display.

Automatic scrolling uses 8, 16, or 24 pixels per second for the slow,
standard, and fast presets respectively.
An illustration is a standalone page and remains visible for the configured
page-turn interval before automatic reading continues. Up/Page Up returns to
the previous page; Down/Page Down advances past the illustration.

Build from the GlassSDK root:

```sh
python3 build.py build --example novel_reader
```

On Windows PowerShell use `py build.py build --example novel_reader`. The GMP
is written to `build-host/.build/novel_reader/novel_reader.gmp`.
