# Example selection and development recipes

Paths are relative to this file. This table routes tasks using the actual source and manifests. Directory links locate the implementation; after choosing an example, read only its relevant README, manifest, and source files.

| Requirement | Preferred references | Architecture and considerations |
| --- | --- | --- |
| Timers, notes, or text status panels | This skill's Web starter; storage/display operations in [permission-debug](../../../../PhoneSDK/examples/permission-debug/) | Public SDK, phone-owned state, and existing web_bridge. Adapt only the needed capabilities, not the entire permission-lab UI. |
| Local native text or arc UI | [lvgl_ui](../../../../GlassSDK/examples/lvgl_ui/), [input](../../../../GlassSDK/examples/input/) | Generate a Glass starter, then extend lifecycle, buttons, and the state machine. |
| Canvas desktop, weather, or charts on the glasses | [life-desk](../../../../PhoneSDK/examples/life-desk/) | Web state with web_bridge. Refer to Canvas-to-GRAY_4 conversion, dirty tiles, and LZ4. Start with one screen instead of copying the entire desktop. |
| TypeScript/Vite or a small board game | [tic-tac-toe](../../../../PhoneSDK/examples/tic-tac-toe/) | package.json and public/manifest.json are source inputs; preview/package dist after building. Preserve relative asset URLs with `base: './'`. |
| Local games controlled by head movement | [breakout](../../../../GlassSDK/examples/game/breakout/), [snake](../../../../GlassSDK/examples/game/snake/), [tetris](../../../../GlassSDK/examples/game/tetris/), [jet_runner](../../../../GlassSDK/examples/game/jet_runner/) | Glass-owned state, IMU, and buttons. No phone rendering is needed; keep game loops nonblocking. |
| Accessory navigation, grids, levels, and undo | [2048](../../../../GlassSDK/examples/game/2048/), [sokoban](../../../../GlassSDK/examples/game/sokoban/) | Select required input capabilities before adapting state, levels, and assets. |
| Multitouch phone control of a glasses game | [fighter-controller](../../../../PhoneSDK/examples/fighter-controller/) + [fighter_arena](../../../../GlassSDK/examples/game/fighter_arena/) | Dedicated paired protocols separate input snapshots from events. Reset input on touch release, disconnection, and expiration. |
| Native animation, image frames, or pets | [image_animation](../../../../GlassSDK/examples/image_animation/), [talking_pet](../../../../GlassSDK/examples/talking_pet/) | Check GRAY_4/transparent indexed formats and asset RAM/Flash budgets. image_animation also includes a shared fighter_arena sprite header. |
| Paired pet with short recording/playback | [talking-pet](../../../../PhoneSDK/examples/talking-pet/) + [talking_pet](../../../../GlassSDK/examples/talking_pet/) | Phone audio/storage and local glasses animation. Send small state packets instead of full frames; do not add a browser microphone fallback. |
| TXT/EPUB reading, progress, and illustrations | [novel-reader](../../../../PhoneSDK/examples/novel-reader/) + [novel_reader](../../../../GlassSDK/examples/novel_reader/) | Host file streams, bounded text windows on the phone, and glasses layout. Start with TXT before EPUB; do not cache whole books or short-lived file tickets. |
| Glasses microphone parameters or real-time streams | [audio-capture-lab](../../../../PhoneSDK/examples/audio-capture-lab/) + [audio_capture_lab](../../../../GlassSDK/examples/audio_capture_lab/) | Audio uses the Host/Web binary stream; GMP receives low-rate UI state only. Do not invent a GMP recording ABI. |
| Optical, brightness, or display-power experiments | [display-control-lab](../../../../PhoneSDK/examples/display-control-lab/) + [display_control_lab](../../../../GlassSDK/examples/display_control_lab/) | Refer to the relevant controls and custom messages. Optical effects require physical-device checks. |
| Basic messages, raw framebuffer, or standalone IMU capability | [bluetooth](../../../../GlassSDK/examples/bluetooth/), [framebuffer](../../../../GlassSDK/examples/framebuffer/), [imu](../../../../GlassSDK/examples/imu/) | Verify individual modules before combining them. Raw framebuffer drawing must separately yield to telephone UI. |

## Examples that need adaptation before use as general templates

`PhoneSDK/examples/app-counter` and `weather` implement App `MemoPluginBridge` callbacks directly; `tic-tac-toe` has its own Bridge adapter. They can provide business or layout references, but ordinary new Web projects should use `createGMPlugin` and the current standalone SDK instead of copying low-level token/generation handling. The older quick-start/developer-guide documents flag outdated permission and audio APIs; current contracts and SDK implementation take precedence.

`PhoneSDK/tools/sync-example-sdk.mjs` currently lists seven fixed example paths and does not discover new vendor directories. The generator bundles the current SDK directly for a new project. Use its `--refresh-sdk` mode when that project needs an update instead of refreshing every existing example.

## Adaptation checklist

Use an independent manifest ID, directory name, and display title. New Web versions are strings, while Glass versions are integers; inherit the ABI from current headers/templates. When copying a dedicated protocol, preserve compatible fields or update both implementations rather than changing channel/version on only one side.

Check C includes such as `../common/call_ui.h`, `../../common/call_ui.h`, and shared asset headers in other examples; they break when directory depth changes. Copy the necessary shared headers into the new project and adjust includes, or preserve and validate the shared paths. Standalone `--project` builds must satisfy review source-root boundaries. This skill's Glass starter includes call_ui.h inside the project.

When copying a Vite project, omit node_modules, dist, caches, and old packages, and inspect public/manifest.json first. For static SDK projects, preserve vendor/ and relative asset paths while removing unused permissions and features from the new project. Document the new application's actual commands and limitations rather than copying the original application's verification claims.
