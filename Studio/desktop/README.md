# GM Plugin Studio Desktop

跨平台桌面 Studio 将 Web 插件和设备 `.gmp` 放在同一个窗口中运行：左侧使用系统
WebView，右侧使用相邻 `previewer/` 的 RV32/Host 模拟核心。

## 当前能力

- macOS 使用 WKWebView，Windows 使用 WebView2，由 Tauri 2 统一封装。
- Web 插件由应用进程内的随机令牌 localhost 服务提供，保证 ES Module、相对资源和插件
  自带 CSP 在两个平台上一致工作；服务只监听 `127.0.0.1`。
- 直接打开 `.mmpkg`，或从目录加载开发中的 Web 插件；入口读取 `manifest.json`，无
  manifest 时回退到 `index.html`。
- 启动时自动扫描 `WebSDK/plugins/*` 和 `WebSDK/examples/*`，优先使用
  `plugins/` 中的完整插件工作区，并兼容 Vite 等项目的 `dist` 或 `build` 产物。启动后
  默认进入“仅设备调试”，不擅自运行 Web 插件。
- 设备插件是 Studio 的必需运行端，Web 插件是可选输入。开发普通 Web 插件且未手动指定
  设备端时，Studio 自动选择并运行 `web_bridge.gmp`；选择“仅设备调试”后会卸载 Web
  视图，但保留设备画面、单击/双击/长按和全部头部手势模拟按钮。
- 用户手动选择设备插件后，Studio 保留该选择，不会因为切换 Web 插件而覆盖，从而继续
  支持 `plugin.sendMessage` 自定义 channel 的双端组合。
- Studio 读取 Web manifest 的 `deviceRequirements` 与设备 manifest 的
  `provides.protocols`，按插件 ID、最低版本和协议版本自动选择设备端，并在下拉框中标记
  “推荐”“兼容”或“不兼容”。手动选择不兼容组合仍可用于诊断，但不会显示为就绪。
- Web 侧可导入仓库外的开发目录或 `.mmpkg` 文件；设备侧会扫描
  `GlassSDK/build-host` 中已构建的 `.gmp`，也可导入外部工作区中的构建产物或单个
  `.gmp` 文件。
- 将当前 Web 插件一键打包为 `.mmpkg`，通过局域网 TCP 服务提供下载并显示二维码。
  二维码和下载协议见 [`../../WebSDK/docs/web-plugin/lan-install.md`](../../WebSDK/docs/web-plugin/lan-install.md)。
- 将当前设备 `.gmp` 通过兼容 SDK 安装工具的 `gmp+tcp` 服务共享并显示二维码，供支持
  设备插件安装的 App 扫码下载。
- 加载并持续运行一个设备 `.gmp`。
- Bridge v1 的 `plugin.sendMessage` 直接调用同进程内的
  `Previewer::sendBluetooth(channel, payload)`。
- Bridge v1 的 `display.*` 会按 Aphrodite Scene 协议编码到默认或开发者自定义的设备
  插件，支持文本、GRAY_4、LZ4 和带 ACK 的原子分块帧；Studio 不按插件名称写死路由。
- 以 30 Hz tick 设备插件，并把 600×350 GRAY_4 framebuffer 显示在右侧。
- 支持 runtime、storage、连接状态查询，以及 button、imuGesture、rawImu、connection
  事件；界面可直接模拟单击、双击、长按和常用头部手势。
- 设备插件通过 Host Bluetooth send 发出的 Scene ACK、标准设备事件和自定义 channel
  都会进入同一 outbox；前两类回到 Bridge，自定义上行消息保留在 Studio 日志中。
- `.mmpkg` 在应用临时缓存中安全解包，拒绝路径穿越、符号链接、超量文件和解压炸弹。

因此同一应用可以覆盖两种组合：

- `gm-life-desk.mmpkg + web_bridge.gmp`：使用完整 `display.*` 和设备事件协议。
- `fighter-controller.mmpkg + fighter_arena.gmp`：使用开发者自定义 channel 的
  `plugin.sendMessage`，不依赖默认 `web_bridge.gmp`。

## 本机运行

需要 Rust、平台 C++ 编译器和 Tauri 对应的系统 WebView。C++ Core 由 Cargo 的 `cc`
构建脚本直接编译，不要求安装 CMake。

```bash
cd Studio/desktop/src-tauri
cargo run
```

打包前安装 Tauri CLI，然后在对应平台生成本机安装产物：

```bash
cargo install tauri-cli --version 2.11.4 --locked
npm run desktop:build
```

“打包并生成二维码”使用与 `WebSDK/tools/build-mmpkg.mjs` 相同的 `.mmpkg v1` 结构和
SHA-256 文件表，生成包写入 SDK 的 `dist/` 目录。手机与电脑必须处于同一局域网；
App 需要实现 `mmpkg+tcp` 调试安装协议。

## 分平台产物

macOS 应在 macOS runner 上构建并签名 `.app/.dmg`；Windows 应在 Windows runner 上使用
MSVC 构建并签名 `.msi/.exe`。两端共享全部 Rust、前端和 C++ Core 源码，现有
`Studio/previewer/src/main_win32.cpp` 继续作为独立 Windows 原生调试器保留。
