> 2026-09-10 Plugin Capability Lab 0.2.8 将短录音迁移到 Web SDK 的 `openRecording()` 便利封装，Host 仅保留统一二进制流；0.2.7 的名称调整保持不变。
>
> 2026-09-10 权限实验室 0.2.6 将锁屏/后台暂停与真正页面卸载拆分：`document.hidden`、`pagehide` 和 Runtime `suspended` 不再主动暂停 H5 音频或停止录音，用于验证宿主的真实锁屏能力。

> 2026-09-08 恢复受控配对通信：共 10 类权限、32 个 Bridge 方法。自定义 H5/设备配对插件声明 device.messaging 和 channels，授权后通过 gm.plugin.sendMessage/onMessage 双向通信；未授权、越界、失效运行态拒绝。标准显示/事件通道仍叠加相应权限。权限实验室只测试九类标准能力，不提供任意消息按钮。通信权限不代表设备插件内部敏感行为已被 App 隔离；真机配对验收待完成。以下历史状态以本段和恢复计划为准。

> 历史记录（已被 0.2.3 取代）：修复版 **0.2.1**，包为 `PhoneSDK/dist/permission-debug-0.2.1.mmpkg`。SDK现要求 `gm.audio.stopCapture(sessionId)`，必须使用openCapture返回的录音ID；实验室已接正常停止/退出清理，并读取captureState.result.recordingId。重新导入新包后验证；旧0.2.0不会自动更新。rawImu/1001/未知方法按钮预期被拒，百度fetch的CORS错误不代表禁网，测试服务需要允许跨域。
> 0.2.0引入十种权限的真实操作页面。使用方式及限制见[实验室说明](../../examples/permission-debug/README.md)，下文0.1.0为历史。

# Bridge 2.0 权限调试（开发分支）

PhoneSDK 与 Desktop Studio 的权限开发分支均已对齐各自最新 `main`。调试和验收必须使用本分支重新生成的 SDK、插件包及 Desktop Studio 安装包。

## 立即调试

在 PhoneSDK 目录执行：

```sh
npm run dev:permissions
```

浏览器打开终端显示的地址（默认 http://127.0.0.1:4173）。当前会话的预览地址是 http://127.0.0.1:4187。

当前 Plugin Capability Lab 0.2.8 的九项权限均为 optional。只批准 storage 时可写入便签，未批准的定位、录音应被拒绝。批准 device.events 后，负向 rawImu 探针仍因范围不符被拒绝。
消息权限需另选 Novel Reader / Fighter Controller / Talking Pet 等自定义配对插件，并选择其匹配设备插件；权限实验室不提供消息按钮。每次启动重新授权。

## Desktop Studio

必须运行本次源码编译的模拟器，不是仓库 Studio/prebuilt 或 /Applications 里的旧版本。
在 plugin_studio 仓库执行（路径换成自己的 checkout）：

```sh
GM_PHONE_SDK_ROOT=/absolute/path/plugin-open-platform/PhoneSDK \
GM_DEVICE_SDK_ROOT=/absolute/path/plugin-open-platform/GlassSDK \
npm run dev:debug
```

Phone 插件下拉选择“Plugin Capability Lab · Bridge 2.0”，完成宿主授权。专用示例不依赖眼镜插件即可调 storage/定位/拒绝路径；显示和真实插件消息需要选择匹配的 Glass 插件。
撤销按钮停止插件并失效文件流会话；刷新/重启后重新申请。

## 打包

```sh
npm run sync:example-sdk
npm run pack:plugin -- examples/permission-debug dist/permission-debug-0.2.8.mmpkg
```

包内 schemaVersion=2、permissionPolicyVersion=1、bridgeVersion="2.0"，权限为严格对象数组。
源码示例 manifest 已迁移，当前版本的标准 dist 包已重新生成。Tic-Tac-Toe 是独立 Vite 工程，修改源码后须在其目录重新 build，旧 dist 不能直接使用。

## 能力和限制

- 声明支持 storage、files.user-selected、display、device.info、device.events、device.messaging、audio.capture、audio.playback、network、location.foreground。
- 32 个方法使用精确白名单；未知方法不因前缀而放行。事件/通道校验 scope，系统通道补查对应 display/device.events。
- SDK 移除 audio.configure/startRecording/stopRecording/onFrames/onState；使用 openCapture/stopCapture/onCaptureState。旧 JSON Opus 帧不作兼容。
- 两种 SDK transport 丢弃旧 generation 响应；重启不会让旧回包完成新请求。
- 位置是显式模拟，不调用电脑定位。getCurrentPosition/watchPosition/clearWatch 可调，隐藏页面后取消监听，无后台功能。
- Desktop 使用电脑麦克风模拟眼镜 Opus 采集，录音数据通过二进制端口交给 H5；回放由插件页面完成，不存在 recordingId 原生回放。
- network 没有 Bridge 方法。授权面板记录其声明/选择，但 Browser/Desktop 此版**没有实现原生禁止联网或强制音频静音**。不要用它验收网络隔离，也不要运行不可信插件。
- storage/文件可用于功能调试，但 Studio 不是 App 的账户、installationId、packageDigest 隔离实现；宿主本身的原生命令也不是生产沙箱。包更新与授权身份持久化需在 App 验收。
- App 通过 Governed Runtime 执行声明与授权校验；网络隔离由 App 容器限制普通 HTTP 请求，不升级或修改 WebView 库，也不覆盖 WebRTC。

## 规则来源与验证

App `docs/plan/plugin_permission_governance/api/permission_contract.md` / `permission_vectors.json`，revision p7-handoff-r1。
PhoneSDK 的 `packages/bridge-contract/src/permission-policy.js` 与 Desktop `desktop/ui/src/permission-policy.js` 是同一份规则快照，各自运行 App 测试向量；更新时必须同步两个文件和向量。
Rust 包解析独立执行同一套声明向量，保留 required、reason、scope，不再只提取 name。

```sh
# PhoneSDK
npm test
npm run check
# plugin_studio
npm test
cargo test --offline --manifest-path desktop/src-tauri/Cargo.toml
```

浏览器人工回归：必需拒绝、最小授权、storage 成功、NOT_GRANTED、UNDECLARED、METHOD_NOT_FOUND、事件/通道 OUT_OF_SCOPE、模拟定位监听/停止、撤销并重新授权。文件选择/眼镜显示/真实音频、三端原生网络隔离仍应分别真机验证。


## 本次验证记录（2026-09-05）

- PhoneSDK：133/133 Node 测试通过；工作区检查及 git diff --check 通过。
- Desktop：118/118 UI 单测、6/6 工具单测、27/27 Rust 测试通过；macOS Universal 2 DMG 构建成功。
- Browser Studio：实际页面完成必需拒绝、最小授权、成功调用、三类权限拒绝、定位事件/停止与撤销重授权。
- macOS Desktop：实际新编译窗口发现并加载“权限调试台”，完成 Bridge 2.0 握手、storage 写入、OUT_OF_SCOPE 与模拟定位返回。
- 当前验证包：PhoneSDK/dist/permission-debug-0.2.8.mmpkg，SHA-256 `f88741fe13852b2533f7dd9aa0a982371c32ab32ca237603a3e7b25b19e8d420`。
- 0.2.5 历史包 SHA-256 为 `8815cfafdb2353f2e19592da66169dd10ddcb3b8b87ccedd68746c521a1e7ba3`，会在页面隐藏时主动暂停播放并停止录音，不得用于锁屏验收。
- 未升级 WebView/Tauri 依赖；当前版本的业务示例包已使用 Bridge 2.0 权限声明重新生成。
- 未验证：真实手机/眼镜、系统定位权限、原生网络和音频隔离、实际文件选择器手工交互。独立 Tic-Tac-Toe 工程的构建与测试尚未执行。
