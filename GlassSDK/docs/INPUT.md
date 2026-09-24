# HOGP Input extension

Studio's Input simulator implements the same ID 4 wire layout and offers mouse,
wheel, KEY, ABS and TOUCH injection. Its native tests run the actual GMP files;
this is separate from real Bluetooth/HID transport validation.

Include `gm_plugin_input.h`. Discover optional extension `GM_PLUGIN_EXTENSION_INPUT`
(ID **4**) using `gm_plugin_input_get(host, &api)`. A missing extension or failed
subscription must not fail Breakout startup: retain ordinary button/IMU controls.
The existing core ABI, Bridge and package format are unchanged. No Bridge version
equality check is introduced.

## Authoritative ABI

The SDK `gm_plugin_extensions.h` is copied from the firmware counterpart in
`Platform/WQ7036/wq-adk/components/apps/acore/displayapp/inc/`. Do not infer the
wire layout from UI requirements. In particular, the final firmware event has
**no class field**: `gm_plugin_input_event_class()` mirrors the Host classifier.
Digitizer source always means TOUCH; otherwise keys 29..32 mean REL, 33..41 ABS,
42..51 TOUCH, and all remaining keys KEY.

The earlier, unpublished Pointer Input draft also used ID 4 but had a different
callback and subscribe signature. It is replaced, not an ABI-compatible alias.
Rebuild draft plugins and update draft firmware together. ID 4 alone cannot
distinguish these two tables; a size check on an event cannot make calling the
wrong subscribe signature safe. Firmware returning ENOTSUP remains supported.
Other published extensions and the core event layout have not changed.

| RV32 member | Type | Offset |
| --- | --- | --- |
| struct_size | uint16_t | 0 |
| key, phase, source, axis | four uint8_t | 2, 3, 4, 5 |
| report_id, reserved0 | two uint8_t | 6, 7 |
| value, norm_value, x, y | four int32_t | 8, 12, 16, 20 |
| modifiers, usage_page, usage_id, reserved1 | four uint16_t | 24, 26, 28, 30 |
| application, timestamp_ms | two uint32_t | 32, 36 |

Event size is **40 bytes** (alignment 4). Accept at least that size and ignore
unknown keys/axes. The callback receives a borrowed sample, valid only during
that invocation: copy it if needed later. Reserved fields are not class bits.

The RV32 function table is **8 bytes**, containing subscribe at offset 0 and
unsubscribe at offset 4:

```c
gm_plugin_result_t (*subscribe)(gm_plugin_input_classes_t classes,
                                gm_plugin_input_callback_t callback,
                                void *context);
void (*unsubscribe)(void);
/* callback: void (*)(void *context, const gm_plugin_input_event_t *event) */
```

Class mask is uint32_t: KEY=1, REL=2, ABS=4, TOUCH=8, ALL=15. Phases are
DOWN=0, UP=1, REPEAT=2, MOVE=3, CANCEL=4. Sources are GENERIC=0, KEYBOARD=1,
MOUSE=2, CONSUMER=3, DIGITIZER=4, GAMEPAD=5, VENDOR=6. Axes are NONE=0,
X/Y/Z=1/2/3, RX/RY/RZ=4/5/6, WHEEL=7, HWHEEL=8, HAT_X/HAT_Y=9/10.
Modifiers bits 0..7 are left Ctrl/Shift/Alt/GUI then right Ctrl/Shift/Alt/GUI.
All 61 key constants and other enum values are checked in `tests/input_abi_asserts.h`,
along with every event field offset and function table layout. The RV32 build's
`abi_check.c` includes these checks; native tests exercise the same assertions.

## Lifecycle and routing

- Subscribe in on_start/on_resume. Registering again replaces callback and mask.
- Subscribed classes bypass cooked local input routing. Avoid processing both
  raw and cooked events as if they were two different physical actions.
- The phone-side mirror is independent. Continuous input is best-effort and can
  be rate-limited; this is not a lossless pointer or recording protocol.
- Host pauses delivery under overlays and restores it on resume. Examples clear
  local active state while suspended and resubscribe on resume.
- Both examples explicitly unsubscribe in on_stop, even after suspension. The
  Host also revokes on failed startup, abnormal termination and unload.
- unsubscribe is idempotent and restores cooked handling. Do not call a saved
  function pointer after unload.
- subscribe returns OK, EINVAL (empty/unknown mask or invalid callback), or
  ESTATE (not in a visible lifecycle). Discovery can return ENOTSUP.

## Value semantics

REL value is a signed delta. Wheel positive means scroll up. ABS carries both
raw value and norm_value. Firmware normalizes centered joystick/gamepad axes to
[-1000,1000]; other absolute devices may retain raw units in norm_value. There
are no logical min/max fields, so arbitrary absolute devices cannot be universally
mapped to screen coordinates without a device profile/calibration.

TOUCH includes x/y for coordinate-bearing events; digitizer axis reports may
instead carry a single coordinate in value. Pressure-capable events use
value/norm_value, but not every touch sample is pressure: inspect key/axis/usage.
TOUCH_TAP is the Host-recognized gesture; a DOWN pulse confirms in Breakout.
Raw DOWN/UP contacts alone are not assumed to be taps (no accidental pause after
a swipe). CANCEL/UP reset the game's touch tracking.

## Examples and verification

- [Breakout](../examples/game/breakout/README.md): raw input plus legacy fallback.

From repository root:

```sh
python GlassSDK/build.py build --example game/breakout
CC=gcc CFLAGS=-fsanitize=address,undefined python GlassSDK/tests/test_input_native.py
GM_XIP_TEST_CC=/path/to/riscv-gcc python -m unittest discover -s GlassSDK/tests -p 'test_*.py'
```

Native tests use fake Host/LVGL services and do not validate real HID radio
delivery or actual glyph rendering. On supported glasses, check KEY down/up/repeat,
both REL axes and wheels, ABS X/RX extremes, TOUCH down/move/up/cancel/pressure,
all HID metadata, overlay recovery, exit/relaunch and subscription cleanup. On
old firmware verify Breakout starts without Input and keeps ordinary controls.
