# System UI notifications

`GM_PLUGIN_EXTENSION_SYSTEM_EVENTS` (ID 5) publishes an immutable `subscribe(on_event,
context)` / `unsubscribe()` table discovered with `gm_plugin_system_events_get()` from
`gm_plugin_system_events.h`. The existing core `gm_plugin_event_t` and plugin descriptor
are unchanged. Older firmware returns `GM_PLUGIN_ENOTSUP`.

Subscribe during `on_start` or `on_resume`. The Host validates the callback,
retains one subscription per plugin and synchronously sends the current call UI
state. A replacement subscription overwrites the previous one. Host stop/unload
revokes delivery before releasing plugin resources. Events and callbacks are
serialized on the display task; an event pointer is borrowed only during its
callback. Notifications continue when a system overlay covers the plugin.

`GM_PLUGIN_SYSTEM_EVENT_CALL_UI` contains `struct_size`, `type`, `timestamp_ms`
and `active`. `active=true` is sent after successful telephone application
allocation and before its UI can draw. `active=false` is sent after telephone UI
resources are destroyed. This reports the phone UI lifecycle, not each HFP call
state; it stays true through ringing, answering, call waiting and phone UI updates
until that application exits. No phone number, contact or caller data is exposed.
On exit, subscribers restore visibility/mark redraw in the callback. The Host
then completes pending LVGL refreshes before the next plugin loop can write raw
pixels, avoiding an LVGL refresh overwriting the first restored frame. Schedule
raw framebuffer redraw for `on_loop`, rather than inside the notification.
Starting a plugin during a call also defers the Host's initial background refresh
until the plugin has had the opportunity to subscribe and hide its LVGL root.

The Host does not block framebuffer locks, stop loops or force a drawing policy.
Developers decide whether to yield. To keep system telephone content visible,
check a plugin-owned flag in **every** render path, including redraws from button
handlers; changing only simulation pause does not stop drawing.

```c
static bool call_ui_active;
static void on_system_event(void *context, const gm_plugin_system_event_t *event)
{
    (void)context;
    if (event && event->struct_size >= sizeof(*event) &&
        event->type == GM_PLUGIN_SYSTEM_EVENT_CALL_UI)
        call_ui_active = event->active;
}
/* on_start: events->subscribe(on_system_event, context);
 * render only: if (call_ui_active) return;
 * on_stop: events->unsubscribe(); */
```

Keep `on_loop` and Bluetooth handlers running; only the drawing function returns
early. Apply the same rule to direct framebuffer writes and LVGL updates or
animations owned by the plugin.

[Fighter Arena v31](../examples/game/fighter_arena/README.md) demonstrates this
policy. It stops all framebuffer writes/presents for the call while continuing
its loop, simulation, input handling and Bluetooth event queue. On UI exit it
redraws in the next loop without resetting input or discarding queued messages. Firmware without
the extension still runs Fighter but cannot notify it of calls. Current Studio
without system-event support cannot simulate this notification; validate on the
updated glasses firmware. From `GlassSDK`, run:

```sh
python3 -m unittest discover -s tests -p test_system_native.py
```

## Other display examples

The other 17 display examples opt in through
[`examples/common/call_ui.h`](../examples/common/call_ui.h). This is private
example code, not an addition to a published SDK interface. Each example starts
its subscription before creating UI or drawing pixels and unsubscribes on stop.
A synchronous initial notification covers starting a plugin during a call.
The extension-only example has no display and needs no call policy.

The helper hides the plugin's LVGL root for the call, so child labels, animation
objects and game state can keep updating without rendering over the telephone
UI. It shows that same root again on call UI exit. It never pauses `on_loop`,
input handling or Bluetooth. This only controls these examples' LVGL tree;
code using raw framebuffer pointers still has to cooperate separately.

Framebuffer, IMU and Talking Pet check the flag before writing pixels and
schedule a redraw on UI exit. Novel Reader still validates and acknowledges
image tiles during the call; after any in-flight transfer completes it requests
the current image again through the existing phone protocol, retrying if the
Bluetooth send queue is busy. If the interrupted transfer stops making progress,
an 8-second idle timeout lets restoration request a new transfer instead of
waiting forever (the phone's tile ACK timeout is 6 seconds). Its text state and
progress messages stay live.

Web Bridge still validates/decompresses bitmap tiles, advances frame counters,
acknowledges transfers and handles ping/input/IMU events while yielding pixels.
Its retained LVGL scene reappears on UI exit. Bitmap content is streamed and has
no full-frame cache: it resumes with subsequent phone drawing commands, so a
static bitmap requires the phone to send it again. A frame accepted during a
call is consumed without presentation, rather than stalled waiting for display.
If a call interrupts an atomic frame, its remaining tiles are also consumed
without presentation, even after call UI exit. Drawing resumes at a fresh frame
begin, so only a complete new frame can be presented.
No phone protocol or phone implementation changes are required for this policy.

The native test runs the real entry/start/loop/stop callbacks of every display
example against a mock Host. It covers starting during a call, unchanged
framebuffer bytes, Bluetooth replies, call-end redraw and old-firmware fallback.
Actual call UI composition still requires validation on updated glasses firmware.
