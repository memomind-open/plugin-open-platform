---
name: memomind-web-plugin
description: 开发或修复 MemoMind PhoneSDK H5 插件、JavaScript Bridge、权限、生命周期、渲染和 Browser Studio。用于 Web 插件与公共 SDK 源码，不用于官网 Portal 或原生眼镜 ABI 实现。
---

# Web 插件与 Bridge

先读 [PhoneSDK 指南](../../../PhoneSDK/README.md) 和 [能力权限契约](../../../PhoneSDK/docs/web-plugin/capability-contract.md)。[quick start](../../../PhoneSDK/docs/web-plugin/quick-start.md) 仅用于目录与运行步骤，其标注的旧权限/音频示例不能当当前 API 合同。新应用先按 [应用开发技能](../memomind-example-app/SKILL.md) 创建起点。路径相对本文件；下列 npm 命令在 `PhoneSDK/` 执行。

## 选择修改位置

插件业务放 `examples/<plugin>/`，优先参考接近需求的现有示例。修改公共 API 前读 [API reference](../../../PhoneSDK/docs/web-plugin/api-reference.md) 并检查 `packages/web-sdk` 和 `bridge-contract` 的实现与测试。

公共包职责见 [packages 导航](../../../PhoneSDK/packages/README.md)：契约被 web-sdk 和 renderer 使用，studio-runtime 使用契约和 renderer。版本以当前能力契约与源码为准，不使用旧权限字符串数组。

- 页面与绘制：读 API reference 及 display-control-lab，核对 GRAY_4、位图大小、LZ4 和原子帧限制。
- 跨端消息：读 [application messaging](../../../PhoneSDK/docs/web-plugin/application-messaging.md)，参考使用 SDK 的 fighter-controller / life-desk；app-counter 直接操作 App Bridge，不能作为通用宿主模板；配对与版本看 [compatibility](../../../PhoneSDK/docs/web-plugin/compatibility.md)。
- 录音、文件、定位或权限：读对应 API 契约和 [permission debugging](../../../PhoneSDK/docs/web-plugin/permission-debug.md)，定位另读 [location](../../../PhoneSDK/docs/web-plugin/location.md)。
- 宿主与重启：读 [runtime and lifecycle](../../../PhoneSDK/docs/web-plugin/runtime-and-lifecycle.md)、[Studio](../../../PhoneSDK/docs/web-plugin/studio.md)。

## 实现规则

通过公开 `gm.*` API 使用 Bridge，不能把 `device.messaging` 当原始 GATT 或安装器通道。当前 manifest 用对象形式权限，声明实际使用的事件 types 与消息 channels；从运行时能力判断是否可用，声明不是授权。

必需权限与可选权限分别处理；拒绝、撤销、不支持或断连要有可用的错误/降级路径。不要缓存旧授权结果作为永久许可。

处理 suspended/stopped/failed 及 runtime 重建；清理订阅、定时器和异步任务。旧 session/generation 的响应与事件不得进入新页面。App WebView 与 Studio 使用不同传输，但插件保持同一公开 API。

修改 SDK 后检查示例的 vendored SDK 是否受影响，固定的七个维护示例按需执行 `npm run sync:example-sdk`，审查生成差异。新增项目不会被该命令发现；使用应用技能生成器 `--refresh-sdk <project>` 单独更新它，避免新应用仍运行旧 SDK。

## 验证与交付

按修改选择现有 Node 测试，例如 `node --test packages/web-sdk/test/sdk.test.mjs`；公共契约或跨包改动运行 `npm run verify`（tests + workspace check）。依赖需要安装时沿用 lockfile 执行 `npm ci`，不顺带升级。

从 PhoneSDK 运行 `node tools/run-browser-studio.mjs --plugin examples/<new-name>` 检查新程序；`npm run dev` 固定打开 app-counter，`npm run dev:permissions` 固定打开 permission-debug，不能据此验证新程序。它不执行 GMP。双端联调使用预编译 Desktop Studio，真机权限、蓝牙及生命周期行为另行记录。

打包输入必须是可部署 H5 目录；读取 [package format](../../../PhoneSDK/docs/web-plugin/package-format.md) 和交付技能。不要把项目 src、node_modules 或旧包当新的交付产物。
