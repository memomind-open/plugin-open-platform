# Memo Novel Reader

This glasses plugin receives bounded UTF-8 windows from the paired Web plugin,
wraps them with the firmware font, and performs continuous vertical scrolling
locally. Novel text is never written to persistent storage. Only the two
visible pages and one incoming text window exist in volatile memory, and all
buffers are cleared on disconnect, suspend, stop, or exit.

The reading view has no title or border. Its only status line shows the exact
directory title at bottom left and reading progress at bottom right.

Automatic scrolling batches display movement to 10 frames per second and only
updates the progress label when its percentage changes, reducing long-running
CPU load and repeated label allocations without changing reading speed.
The centered reading viewport displays five scrolling text lines at a time.
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

Automatic scrolling uses 8, 16, or 24 pixels per second for the slow,
standard, and fast presets respectively.

Build from the GlassSDK root:

```sh
python3 build.py build --example novel_reader
```

On Windows PowerShell use `py build.py build --example novel_reader`. The GMP
is written to `build-host/.build/novel_reader/novel_reader.gmp`.
