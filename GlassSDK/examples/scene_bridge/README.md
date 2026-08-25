# GM scene display bridge

This example keeps application rendering policy on the phone while executing the
device-specific drawing locally as one GM plugin. It receives phone-to-glasses
plugin command `0x28` (`INT16 channel`, `BYTES payload`) and maps scene elements to the
firmware-owned LVGL runtime. The phone sends every logical display operation
once; duplication across physical outputs remains a firmware responsibility.

Channels and big-endian payloads:

| Channel | Payload |
| ---: | --- |
| `1` clear | any non-empty byte payload |
| `2` text | `id:u8 x:u16 y:u16 w:u16 h:u16 border:u8 radius:u8 utf8...` |
| `3` rect | `id:u8 x:u16 y:u16 w:u16 h:u16 border:u8 radius:u8` |
| `4` delete | `id:u8` |
| `5` line | `id:u8 x1:u16 y1:u16 x2:u16 y2:u16 width:u8` |
| `6` GRAY_4 tile | `x:u16 y:u16 w:u16 h:u16 stride:u16 pixels...` |
| `0x7FFE` ping | opaque bytes, echoed back with glasses-to-phone command `0x29` |

The tile channel is the navigation/image escape hatch. It uses the system
framebuffer directly and does not allocate a second full-screen buffer. Keep
its pixels separate from active LVGL objects because a later LVGL invalidation
can redraw an overlapping region.

After changing this example, run `./gm-build` from the SDK root.

The phone-side adapter only needs to translate scene operations into these
channels. Lifecycle remains the normal plugin install/start/stop/remove flow;
phone and plugin can be updated together without baking application-specific
logic into firmware ROM.
