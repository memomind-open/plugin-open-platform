# Display application capability matrix

The plugin ABI models reusable hardware services, not LVGL widgets or existing
product applications. An application is assembled from the frozen core Host
table plus independently queried extensions.

| Application type | Plugin-side composition | Host services |
| --- | --- | --- |
| Breakout, Tetris, Jet Runner | game loop, collision, sprites, drawing | direct framebuffer, button/IMU, monotonic time, optional locale |
| Launcher, welcome, shortcut, system UI | layout, cards, icons, text | shared LVGL, device state, input, display control |
| Navigation, taxi | route/card renderer and phone protocol | LVGL, BT messages, IMU |
| Prompter, translation, subtitles | text layout/scrolling and phone protocol | shared LVGL, BT messages, IMU, display control |
| AI QA, meeting, voiceprint | application state machine and phone media protocol | LVGL, BT messages; authorized PhoneSDK capture uses the native recording stream |
| Phone-call UI | call state presentation and controls | LVGL, BT/system events; audio uses phone HFP |
| Head calibration | calibration state machine | LVGL, raw IMU, display control; calibration/storage remain privileged |
| Notifications/dashboard | card/layout modules and cached data | LVGL, BT, device state; persistent KV is not yet exposed |

Per-pixel plugins lock a Host-owned GRAY_4 framebuffer slice and draw into it
directly; neither side allocates another framebuffer. Text uses LVGL labels,
rectangles use `obj_create` plus
background/border styles, and lines use the firmware-owned `line_create` and
`line_set_points` entries.

`graphics.framebuffer.lock(y, &surface)` returns the complete synchronization
slice containing `y`; its boundaries are Host-selected rather than ABI-fixed.
After direct drawing, `unlock(&dirty, false)` submits a non-final slice without
presenting it. A renderer then locks the next returned y range while SPI reads
the previous one, and passes `true` only for the final slice. This provides
temporal double buffering and one atomic presentation without holding or
allocating the whole screen twice.

Direct framebuffer access and LVGL have independent synchronization. Never
invoke LVGL between `framebuffer.lock` and `framebuffer.unlock`; the LVGL flush
path may need the same slice and a later LVGL redraw can replace overlapping
direct pixels. Use LVGL for normal UI. Use direct framebuffer access for a
specialized per-pixel screen, or use the two only in separate, non-overlapping,
non-nested phases.

For UI-heavy applications, `host->graphics.lvgl` calls the firmware's existing LVGL
through the required core drawing table. No LVGL engine, draw buffer, font or
timer loop is copied into the GMP. Small games and custom renderers can still
use the direct Host tile path when per-pixel access is required.

Localized plugins obtain the current BCP-47-style language tag through the
frozen core `locale_get` capability and keep their own translated strings.
This avoids coupling plugin binaries to firmware UI resources.

Factory testing, firmware/font OTA, raw flash access, charging control and
power-off are deliberately not exposed as Host services. GMP currently runs as
trusted native code without an MPU sandbox or package signature, so production
software must not install packages from an untrusted source.

Mic and speaker PCM are intentionally not part of the glasses plugin ABI.
Glasses plugins send control/state through Bluetooth business messages. Phone
plugins with `audio.capture` can use `gm.audio.openCapture()` to receive the
native glasses Opus stream; this is not an HFP-only capture contract. Playback
is handled by the phone/WebView and its policy; phone-call HFP is a separate path.
See the [PhoneSDK audio API](../../PhoneSDK/docs/web-plugin/api-reference.md#native-glasses-audio).
