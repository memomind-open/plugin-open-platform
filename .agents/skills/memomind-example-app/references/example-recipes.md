# 示例选型与开发路线

路径相对本文件。此表根据源码与 manifest 路由，表内目录链接用于定位，选中后只读取相关 README、manifest 和实现文件。

| 需求 | 首选参考 | 架构与注意点 |
| --- | --- | --- |
| 计时器、便签、文字状态面板 | 本技能生成的 Web 工程；[permission-debug](../../../../PhoneSDK/examples/permission-debug/) 的 storage/display 操作 | 用公开 SDK，手机状态 + 现有 web_bridge；只移植所需能力，不复制整套权限实验 UI |
| 本地原生文字/圆弧 UI | [lvgl_ui](../../../../GlassSDK/examples/lvgl_ui/)、[input](../../../../GlassSDK/examples/input/) | 本技能生成 Glass 工程后扩展生命周期、按键与状态机 |
| 眼镜端 Canvas 桌面、天气、图表 | [life-desk](../../../../PhoneSDK/examples/life-desk/) | Web 状态 + web_bridge，参考 Canvas→GRAY_4→脏块/LZ4；先做单屏，不复制整个桌面系统 |
| TypeScript/Vite、小型棋盘游戏 | [tic-tac-toe](../../../../PhoneSDK/examples/tic-tac-toe/) | package.json 和 public/manifest.json 是源，构建后预览/打包 dist；保留 `base: './'` 的相对资源路径 |
| 头部动作控制本地游戏 | [breakout](../../../../GlassSDK/examples/game/breakout/)、[snake](../../../../GlassSDK/examples/game/snake/)、[tetris](../../../../GlassSDK/examples/game/tetris/)、[jet_runner](../../../../GlassSDK/examples/game/jet_runner/) | Glass 自主状态、IMU 和按钮，不需要手机绘制；游戏循环不阻塞 |
| 配件方向键、网格、关卡与撤销 | [2048](../../../../GlassSDK/examples/game/2048/)、[sokoban](../../../../GlassSDK/examples/game/sokoban/) | 先选所需输入能力，再移植状态、关卡与素材 |
| 手机多点触摸控制眼镜游戏 | [fighter-controller](../../../../PhoneSDK/examples/fighter-controller/) + [fighter_arena](../../../../GlassSDK/examples/game/fighter_arena/) | 专用双端协议，控制快照与事件分开；释放触摸、断连、过期输入需复位 |
| 原生动画、图片帧、宠物 | [image_animation](../../../../GlassSDK/examples/image_animation/)、[talking_pet](../../../../GlassSDK/examples/talking_pet/) | GRAY_4/透明索引格式、资产与 RAM/Flash 预算；image_animation 还引用 fighter_arena 的共享素材头文件 |
| 双端宠物与短录音回放 | [talking-pet](../../../../PhoneSDK/examples/talking-pet/) + [talking_pet](../../../../GlassSDK/examples/talking_pet/) | 手机音频/存储，眼镜本地动画；发送小状态包，不发送整帧；不要改成浏览器麦克风兜底 |
| TXT/EPUB 阅读、进度、插图 | [novel-reader](../../../../PhoneSDK/examples/novel-reader/) + [novel_reader](../../../../GlassSDK/examples/novel_reader/) | Host 文件流、手机有界文本窗口、眼镜排版；先做 TXT 再扩 EPUB，不缓存整本书或短期文件票据 |
| 眼镜麦克风参数/实时流 | [audio-capture-lab](../../../../PhoneSDK/examples/audio-capture-lab/) + [audio_capture_lab](../../../../GlassSDK/examples/audio_capture_lab/) | 音频走 Host Web 二进制流，GMP 只接低频 UI 状态；不能从 GMP 虚构录音 ABI |
| 光学/亮度/显示开关实验 | [display-control-lab](../../../../PhoneSDK/examples/display-control-lab/) + [display_control_lab](../../../../GlassSDK/examples/display_control_lab/) | 相关控制与自定义消息参考；光学效果仍需真机检查 |
| 裸消息、raw framebuffer、IMU 单能力 | [bluetooth](../../../../GlassSDK/examples/bluetooth/)、[framebuffer](../../../../GlassSDK/examples/framebuffer/)、[imu](../../../../GlassSDK/examples/imu/) | 先验证单模块，再组合；raw framebuffer 必须单独处理电话 UI 避让 |

## 哪些示例不能直接当通用模板

`PhoneSDK/examples/app-counter`、`weather` 直接实现 App `MemoPluginBridge` 回调，`tic-tac-toe` 有自己的 Bridge 适配层。它们可提供业务/布局参考，但新普通 Web 工程优先用 `createGMPlugin` 与当前 standalone SDK，避免照抄底层 token/generation 处理。旧 quick-start/developer-guide 已标注过时权限与音频接口，当前契约和真实 SDK 优先。

`PhoneSDK/tools/sync-example-sdk.mjs` 目前只有固定的七个示例路径，新增目录不会自动更新 vendor。生成工具直接为新工程捆绑当前 SDK；需要刷新时用该工具的 `--refresh-sdk` 模式。不要为了刷新新程序先全量更新旧示例。

## 移植清单

新程序取独立 manifest ID 和目录名，产品标题独立；新 Web 版本为字符串，Glass 版本为整数，ABI 从当前头文件/模板继承。复制专用协议时保留兼容字段或同时更新两端，不只改一端的 channel/version。

检查 C 的 `../common/call_ui.h`、`../../common/call_ui.h` 和跨例子素材 include；目录深度改变后它们会失效。可复制需要的共享头文件到新项目并改 include，或保持并验证共享路径。独立 `--project` 必须满足审核 source-root 边界；本技能的 Glass 起点把 call_ui.h 放进项目内。

复制 Vite 工程不要带 node_modules、dist、缓存、旧包；先检查 public/manifest.json。复制静态 SDK 工程保留 vendor/ 与资源相对路径，清理新工程里不需要的权限与功能。输出 README 写新程序实际的命令与限制，不照抄原程序的已验证结论。
