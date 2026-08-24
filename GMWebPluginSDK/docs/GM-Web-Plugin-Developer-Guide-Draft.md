# GM Web 插件开发指南

> 文档状态：内部体验草稿
> DevKit 版本：0.1.0
> Bridge 版本：1.0
> 运行环境：Node.js 18 或更高版本

## 1. 文档说明

GM Web 插件是运行在手机 App WebView 中的 H5 应用。插件可以展示自己的手机端页面，并通过 GM Web Plugin SDK 调用 App 提供的能力，例如：

- 在眼镜屏幕上绘制文字和图片；
- 接收眼镜按键和头部动作；
- 获取设备连接状态；
- 保存插件私有数据；
- 感知插件运行生命周期。

当前为内部体验阶段，通过 `gm-web-plugin-devkit-0.1.0.zip` 提供 SDK、Studio 模拟器、插件打包器、示例和文档。正式发布后计划改为 npm 包，Bridge API、manifest 和 `.mmpkg` 格式保持不变。

## 2. DevKit 包含内容

解压后的目录结构如下：

```text
gm-web-plugin-devkit-0.1.0/
├── README.md
├── GM-Web-Plugin-Developer-Guide-Draft.md
├── DEVKIT-MANIFEST.json
├── package.json
├── sdk/
│   ├── gm-plugin-web-sdk.esm.js
│   └── gm-plugin-web-sdk.d.ts
├── studio/
│   └── gm-plugin-studio.mjs
├── tools/
│   └── build-mmpkg.mjs
├── examples/
│   └── counter/
└── internal/
```

各目录用途：

- `sdk/`：App 与 Web 插件交互的浏览器 ESM SDK，以及 TypeScript 类型定义。
- `studio/`：模拟手机 App、眼镜屏幕、设备事件和生命周期的本地开发工具。
- `tools/`：将最终 H5 构建产物打包为 `.mmpkg`。
- `examples/`：已经使用本地 SDK 的可运行示例。
- `internal/`：Studio 内部依赖，插件开发者不需要修改或直接引用。

## 3. 五分钟运行示例

### 3.1 解压 DevKit

```sh
unzip gm-web-plugin-devkit-0.1.0.zip
cd gm-web-plugin-devkit-0.1.0
```

### 3.2 启动 Counter 示例

```sh
node studio/gm-plugin-studio.mjs --plugin examples/counter
```

浏览器打开：

```text
http://127.0.0.1:4173
```

可以在 Studio 中进行以下操作：

1. 点击插件页面中的“绘制到眼镜”。
2. 确认右侧虚拟眼镜显示绿色文字。
3. 点击单击、双击、长按或头部动作按钮。
4. 确认插件页面收到对应设备事件。
5. 切换设备连接和生命周期状态。
6. 在 Bridge Inspector 中查看请求、响应和事件。

如果 4173 端口被占用，可以指定其他端口：

```sh
node studio/gm-plugin-studio.mjs \
  --plugin examples/counter \
  --port 4174
```

## 4. 创建自己的插件

一个最小插件可以采用以下结构：

```text
my-plugin/
├── index.html
├── manifest.json
├── plugin.js
├── style.css
└── vendor/
    └── gm-plugin-web-sdk.esm.js
```

如果使用 Vite、Webpack 等工具，最终的 `dist/` 目录也必须包含这些运行文件，并且所有资源使用包内相对路径。

不要在最终插件中引用：

- 开发电脑上的绝对文件路径；
- DevKit 解压目录的绝对路径；
- `node_modules` 中未被构建或复制到 `dist` 的文件；
- Studio 专用的 `/sdk/`、`/runtime/` 等内部地址。

## 5. 引入本地 SDK

### 5.1 复制 SDK

在当前 ZIP 分发阶段，把 SDK 文件复制到插件工程：

```sh
mkdir -p ./vendor
cp /path/to/gm-web-plugin-devkit/sdk/gm-plugin-web-sdk.esm.js \
  ./vendor/
```

如果使用 Vite，可以复制到会原样进入最终构建目录的静态资源目录，例如：

```sh
mkdir -p ./public/vendor
cp /path/to/gm-web-plugin-devkit/sdk/gm-plugin-web-sdk.esm.js \
  ./public/vendor/
```

构建后需要确认：

```text
dist/vendor/gm-plugin-web-sdk.esm.js
```

确实存在。

### 5.2 初始化插件

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();

try {
  await gm.ready();
  console.log('GM Plugin Bridge 已就绪');
} catch (error) {
  console.error(error.code, error.message);
}
```

`createGMPlugin()` 会自动识别宿主：

- 在真实 App WebView 中使用 `MemoPluginBridge`；
- 在 Studio 中使用模拟 Bridge；
- 插件业务代码不需要为 App 和 Studio维护两套实现。

插件结束运行时可以释放资源：

```js
gm.close();
```

## 6. manifest 配置

H5 构建输出目录根部必须包含 `manifest.json`：

```json
{
  "id": "com.example.weather",
  "name": "天气插件",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": [
    "display",
    "device.events",
    "storage"
  ]
}
```

字段说明：

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `id` | 是 | 插件唯一 ID，使用反向域名格式，最大 128 字符 |
| `name` | 是 | 插件展示名称，最大 80 字符 |
| `version` | 是 | 语义化版本，例如 `1.0.0` |
| `entry` | 是 | 包内入口 HTML 相对路径 |
| `bridgeVersion` | 是 | 当前固定为 `1.0` |
| `permissions` | 是 | 插件申请的 App 能力，最多 16 项，不能重复 |

`entry` 必须满足：

- 指向 `.html` 文件；
- 不能是绝对路径；
- 不能包含反斜杠；
- 不能包含空路径段、`.` 或 `..`；
- 最大 256 字符。

当前权限：

| 权限 | 能力 |
| --- | --- |
| `display` | 创建、更新和关闭眼镜显示页面 |
| `device.events` | 订阅设备按键、头部动作、连接状态和 IMU 事件 |
| `storage` | 使用当前插件隔离的 App 键值存储 |
| `network` | 声明插件需要网络访问 |

App 会在 Bridge 调用时检查 `display`、事件订阅使用的 `device.events` 和 `storage`。
`device.getInfo()` 与 `plugin.sendMessage()` 不要求 manifest 权限。插件不应声明未使用的权限。

当前 Debug 版本中的 `network` 主要是声明信息，不代表 App 已完成网络域名隔离。需要访问网络的插件仍应使用严格 CSP，只连接必要域名。

## 7. Runtime API

### 7.1 等待宿主就绪

```js
await gm.ready();
```

插件应在 `ready()` 完成后再调用其他 Bridge API。

### 7.2 心跳检测

```js
const result = await gm.runtime.ping();
```

用于确认当前 App Runtime 仍然可用。

### 7.3 获取 Bridge 版本

```js
const result = await gm.runtime.getBridgeVersion();

console.log(result.version);
```

### 7.4 获取宿主能力

```js
const capabilities = await gm.runtime.getCapabilities();
```

插件应以宿主返回的能力为准，不要仅根据 SDK 版本假设所有能力都可用。

### 7.5 获取生命周期

```js
const result = await gm.runtime.getLifecycleState();

console.log(result.state);
```

## 8. Storage API

Storage 是由 App 提供的插件私有键值存储。不同插件必须按插件 ID 隔离。

### 8.1 写入

```js
await gm.storage.set('game-state', {
  score: 10,
  level: 2,
});
```

### 8.2 读取

```js
const result = await gm.storage.get('game-state');

console.log(result.value);
```

不存在的 key 返回：

```js
{ value: null }
```

### 8.3 删除

```js
await gm.storage.remove('game-state');
```

### 8.4 清空

```js
await gm.storage.clear();
```

建议只保存 JSON 可序列化的数据，不要保存 DOM、函数或循环引用对象。

## 9. Display API

手机 WebView 中显示的 HTML 不会自动出现在眼镜中。只有通过 `gm.display.*` 提交的内容才会进入眼镜显示画面。

当前设备 Profile：

```text
尺寸：600 × 350
刷新率：30 Hz
像素格式：GRAY_4
Studio 显示：黑色到绿色的 16 级亮度映射
```

### 9.1 蓝牙单包限制

眼镜画面通过 Scene Bridge 经蓝牙发送，单次 payload 不能超过：

```text
81,901 B
```

不同绘图通道的实际限制：

| 通道 | 内容 | 协议头 | 单次内容上限 | 额外限制 |
| --- | --- | ---: | ---: | --- |
| Channel 2 | UTF-8 文字 | 11 B | 81,890 B | 包含文字元素参数 |
| Channel 6 | 原始 GRAY_4 | 10 B | 81,891 B | `pixels.length = stride × height` |
| Channel 7 | raw LZ4 GRAY_4 | 14 B | 压缩数据 81,887 B | 解压后位图也不能超过 81,901 B |
| Channel 8 | 原子帧开始 | 6 B | 不适用 | `frameId + tileCount`，最多 256 块 |
| Channel 9 | 原子帧 raw LZ4 GRAY_4 | 20 B | 压缩数据 81,881 B | 按 `tileIndex` 顺序发送，最后一块才显示 |

600×350 整屏 GRAY_4 的原始大小为：

```text
stride = 600 / 2 = 300 B
decodedSize = 300 × 350 = 105,000 B
```

因此整屏不能作为一个 Channel 6 请求发送，也不能仅通过 LZ4 压缩后作为一个 Channel 7 请求发送，因为 Channel 7 同时限制解压后大小。

对于宽 600、stride 300 的整屏：

- Channel 6 每块最多 272 行，需要拆成 `272 + 78` 两块；
- Channel 7 每块解压后最多 273 行，需要拆成 `273 + 77` 两块，并分别生成独立 raw LZ4 block；
- 如果某块 LZ4 压缩后仍超过 81,887 B，需要继续缩小区块。

推荐优先沿纵向分块，保持每块宽度和 stride 一致：

```js
const MAX_SCENE_PAYLOAD_BYTES = 81901;
const CHANNEL_6_HEADER_BYTES = 10;

async function sendGray4ByRows({
  gm,
  x,
  y,
  width,
  height,
  stride,
  pixels,
  toBase64,
}) {
  const maxRows = Math.floor(
    (MAX_SCENE_PAYLOAD_BYTES - CHANNEL_6_HEADER_BYTES) / stride,
  );

  for (let row = 0; row < height; row += maxRows) {
    const tileHeight = Math.min(maxRows, height - row);
    const start = row * stride;
    const end = start + tileHeight * stride;
    const tile = pixels.subarray(start, end);

    await gm.display.updateImage({
      x,
      y: y + row,
      width,
      height: tileHeight,
      stride,
      dataBase64: toBase64(tile),
    });
  }
}
```

LZ4 分块流程应为：

```text
原始 GRAY_4
  ↓ 按行切成多个独立区块
每个区块分别执行 raw LZ4 压缩
  ↓
需要原子显示时，先调用 display.beginFrame，再逐块调用 display.updateFrameImageLz4
```

不要先压缩整屏后再切割压缩字节；每个 Channel 7 请求必须包含一个能够独立解压的完整 raw LZ4 block。

坐标原点位于眼镜画面左上角：

```text
(0, 0) ───────────────→ x
  │
  │
  │
  ↓ y
```

所有绘制区域必须完整位于 600×350 范围内。

### 9.2 创建页面

```js
await gm.display.createPage();
```

### 9.3 更新文字

```js
await gm.display.updateText({
  id: 1,
  x: 20,
  y: 20,
  width: 300,
  height: 80,
  border: 1,
  radius: 8,
  text: '第一行\n第二行',
});
```

字段说明：

- `id`：文字元素 ID，范围 0 到 255；相同 ID 用于更新同一个元素。
- `x`、`y`：元素左上角坐标。
- `width`、`height`：元素区域尺寸。
- `border`：边框宽度，0 表示无边框。
- `radius`：圆角半径。
- `text`：非空文字内容，支持 `\n` 换行。

Studio 会模拟换行和绿色显示，但浏览器字体并不是眼镜固件字体的像素级替代，最终排版需要真机验收。

### 9.4 更新 GRAY_4 图片

```js
await gm.display.updateImage({
  x: 0,
  y: 0,
  width: 256,
  height: 256,
  stride: 128,
  dataBase64,
});
```

GRAY_4 每个像素使用 4 bit：

- 亮度范围为 0 到 15；
- 偶数位置像素放在一个字节的高 4 bit；
- 奇数位置像素放在低 4 bit；
- `stride` 必须等于 `ceil(width / 2)`；
- `dataBase64` 是打包后字节数组的 Base64 字符串。

### 9.5 更新 LZ4 压缩图片

```js
await gm.display.updateImageLz4({
  x: 0,
  y: 0,
  width: 256,
  height: 256,
  stride: 128,
  decodedSize: 32768,
  dataBase64,
});
```

`decodedSize` 必须等于：

```text
stride × height
```

`dataBase64` 是 raw LZ4 block 的 Base64 字符串。

LZ4 只用于降低传输字节数，不会扩大设备允许的单块解压缓冲。超过 81,901 B 的解压位图仍必须先分块，再对每块独立压缩。

### 9.6 原子更新多个 LZ4 分块

```js
const frameId = 42;
await gm.display.beginFrame({ frameId, tileCount: tiles.length });
for (let tileIndex = 0; tileIndex < tiles.length; tileIndex += 1) {
  await gm.display.updateFrameImageLz4({
    frameId,
    tileIndex,
    ...tiles[tileIndex],
  });
}
```

Begin 和每个 Tile 都会等待设备状态 ACK。`tileIndex` 必须从 0 连续递增；中间分块不会刷新屏幕，设备确认最后一块完成后才整体显示。任一请求失败都应放弃当前帧，并使用新的 `frameId` 从 Begin 开始重传完整画面。这里的 ACK 是设备完成该协议步骤的确认，不是蓝牙物理分片 ACK。

### 9.7 整体重建页面

```js
await gm.display.rebuildPage([
  {
    type: 'text',
    id: 1,
    x: 20,
    y: 20,
    width: 300,
    height: 60,
    border: 1,
    radius: 8,
    text: 'Hello GM',
  },
  {
    type: 'image',
    x: 320,
    y: 20,
    width: 128,
    height: 128,
    stride: 64,
    dataBase64,
  },
]);
```

重建操作会先清空页面，再按顺序执行操作。单次最多 128 个操作。

### 9.7 关闭页面

```js
await gm.display.closePage();
```

逻辑 ACK 只表示 App 或设备接受了请求，不等于已经完成人眼可见的最终验收。

## 10. Device API

### 10.1 获取设备信息

```js
const info = await gm.device.getInfo();

console.log(info.connected);
console.log(info.transport);
console.log(info.profile);
```

`device.getInfo()` 是只读状态查询，不要求 `device.events` 权限。

### 10.2 订阅设备事件

```js
const result = await gm.device.subscribeEvents([
  'button',
  'imuGesture',
  'connection',
]);

const subscriptionId = result.subscriptionId;
```

订阅和取消订阅设备事件要求 `device.events` 权限。

可订阅类型：

```text
button
imuGesture
rawImu
connection
```

不再需要时取消订阅：

```js
await gm.device.unsubscribeEvents(subscriptionId);
```

### 10.3 按键事件

```js
const offButton = gm.device.onButton((event) => {
  switch (event.action) {
    case 'single':
      console.log('单击');
      break;
    case 'double':
      console.log('双击');
      break;
    case 'long':
      console.log('长按');
      break;
  }
});
```

不再监听时：

```js
offButton();
```

### 10.4 头部动作

```js
const offGesture = gm.device.onGesture((event) => {
  if (!event.active) return;

  console.log(event.gesture);
});
```

当前动作名称：

```text
headRaise
headLower
left
right
nod
shake
headRaiseTimeout
headLowerTimeout
```

### 10.5 连接事件

```js
const offConnection = gm.device.onConnection((event) => {
  if (event.connected) {
    console.log('设备已连接');
  } else {
    console.log('设备已断开');
  }
});
```

### 10.6 原始 IMU

```js
const offRawImu = gm.device.onRawImu((event) => {
  console.log(event);
});
```

原始 IMU 已进入 Bridge 契约，但当前 Studio 还没有对应的可视化注入面板。高频 IMU 会增加计算和通信压力，只应在确实需要时订阅。

### 10.7 通用插件消息

向当前运行的设备插件发送一条通用二进制消息：

```js
const result = await gm.plugin.sendMessage(
  0x4647,
  new Uint8Array([0x02, 0x01, 0x00, 0x00]),
);

console.log(result.sent);
console.log(result.payloadBytes);
```

`channel` 必须是 `0..65535` 的整数，payload 必须为非空 `Uint8Array`，最大 81901 字节。
此方法不要求 manifest 权限，也不会安装或切换 GMP；消息只发送给当前运行的设备插件。
成功返回只表示底层 GM 命令已收到 ACK，不表示设备插件的业务逻辑已经处理完成。

## 11. 生命周期

当前生命周期状态：

```text
starting
running
suspended
stopped
failed
```

监听生命周期：

```js
const offLifecycle = gm.on(
  'runtime.lifecycleChanged',
  (event) => {
    console.log(event.state);
  },
);
```

建议处理方式：

- `starting`：等待初始化完成。
- `running`：恢复事件处理和必要的画面更新。
- `suspended`：暂停动画、高频计算和非必要请求。
- `stopped`：保存状态并释放资源。
- `failed`：停止继续请求，记录或展示错误状态。

插件不能假设 WebView 永远常驻。页面重载、Runtime 重建或 App 切换状态后，旧请求和旧事件可能失效。

## 12. 会话与请求模型

宿主启动插件时会建立：

```text
sessionToken
runtimeGeneration
```

SDK 会自动把这些信息放入每次 Bridge 请求中：

```text
version
sessionToken
requestId
method
params
runtimeGeneration
```

作用包括：

- 确保请求属于当前插件会话；
- 防止旧页面调用新的 App Runtime；
- 页面重载后过滤旧事件；
- 通过 `requestId` 将异步响应匹配到正确请求。

插件开发者通常不需要手动构造这些字段，应优先使用 SDK 封装方法。

## 13. 错误处理

Bridge 错误会转换为 `GMPluginError`：

```js
try {
  await gm.display.updateText(params);
} catch (error) {
  console.error(error.code, error.message);
}
```

当前错误码：

| 错误码 | 含义 |
| --- | --- |
| `INVALID_REQUEST` | 参数或请求结构无效 |
| `PAYLOAD_TOO_LARGE` | 请求载荷超过限制 |
| `UNAUTHORIZED` | 会话或权限校验失败 |
| `STALE_RUNTIME` | 请求属于旧 Runtime |
| `METHOD_NOT_FOUND` | 方法不存在或宿主不支持 |
| `RATE_LIMITED` | 请求频率超过限制 |
| `BUSY` | 设备或 Runtime 正忙 |
| `QUOTA_EXCEEDED` | 存储、订阅或其他配额超限 |
| `TIMEOUT` | SDK 或 Bridge 请求超时 |
| `DEVICE_DISCONNECTED` | 设备未连接 |
| `CAPABILITY_UNAVAILABLE` | 当前宿主不具备对应能力 |
| `RUNTIME_CLOSED` | Runtime 已关闭 |
| `INTERNAL_ERROR` | App 或 Studio 内部错误 |

建议：

- 对断连、忙碌和限流做有上限的重试；
- 不要无限循环重试；
- 对 `STALE_RUNTIME` 停止旧任务，等待新页面重新初始化；
- 对参数错误直接修正代码，不要重试；
- 用户可感知的失败应在 H5 页面提供明确提示。

## 14. 使用 Studio 调试自己的插件

### 14.1 纯静态 H5

```sh
node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin
```

插件目录根部必须有 `index.html`。

### 14.2 Vite 或 Webpack 项目

当前 Studio 加载静态目录，不负责启动 Vite，也不支持 HMR。先构建项目：

```sh
npm run build
```

再加载最终构建目录：

```sh
node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin/dist
```

修改代码后重新构建，并点击 Studio 中的“重新加载”。

### 14.3 Studio 可模拟内容

- 600×350 绿色单色眼镜画面；
- 单击、双击、长按；
- 抬头、低头、向左转头、向右转头；
- 设备连接和断开；
- `running`、`suspended`、`stopped`、`failed` 生命周期；
- Bridge 请求、响应和事件日志。
- Channel 2/6/7 的 81,901 B 单包限制；
- Channel 7 的 raw LZ4 解压尺寸限制；
- 最近一次绘图的通道、payload 大小和 LZ4 解压大小。

请求超过限制时，Studio 返回 `PAYLOAD_TOO_LARGE`，不绘制该请求，并提示建议的最大分块行数。插件应在 Studio 中完成分块策略验证后再进行真机测试。

### 14.4 Studio 与真机的差异

Studio 的目标是 Bridge 协议和设备画布一致，不是完整光学仿真。以下内容必须真机验收：

- 固件字体和精确字形；
- 人眼看到的亮度和对比度；
- 镜片畸变、视距和视场；
- 真实设备刷新和传输耗时；
- 手势识别阈值和误触发；
- 固件与 App 具体版本的兼容性。

## 15. 打包最终 `.mmpkg`

`.mmpkg v1` 是 ZIP 容器。插件文件必须直接位于压缩包根目录，不能额外套一层项目目录。

正确结构：

```text
weather-1.0.0.mmpkg
├── manifest.json
├── index.html
└── assets/
    ├── index.js
    └── index.css
```

错误结构：

```text
weather-1.0.0.mmpkg
└── weather-1.0.0/
    ├── manifest.json
    └── index.html
```

### 15.1 执行打包

先生成最终 H5 产物：

```sh
npm run build
```

再执行 DevKit 打包器：

```sh
node /path/to/devkit/tools/build-mmpkg.mjs \
  /absolute/path/to/my-plugin/dist \
  /absolute/path/to/release/my-plugin-1.0.0.mmpkg
```

输入目录必须是最终部署产物，不要把 `src`、测试、工程配置或 `node_modules` 一起打包。

输出文件：

- 必须使用 `.mmpkg` 扩展名；
- 不能放在输入目录内部；
- 建议文件名包含插件 ID 或名称及版本号。

### 15.2 打包器执行内容

打包器会：

1. 校验 manifest 字段、权限和入口文件。
2. 拒绝符号链接及不安全路径。
3. 对所有载荷文件计算 SHA-256。
4. 写入 `schemaVersion: 1`。
5. 生成完整 `files` 哈希表。
6. 检查文件数量和大小限制。
7. 原子生成 ZIP 格式的 `.mmpkg`。

最终 manifest 示例：

```json
{
  "id": "com.example.weather",
  "name": "天气插件",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": [
    "display",
    "device.events",
    "storage"
  ],
  "schemaVersion": 1,
  "files": {
    "assets/index.css": "sha256:<64 位小写十六进制>",
    "assets/index.js": "sha256:<64 位小写十六进制>",
    "index.html": "sha256:<64 位小写十六进制>"
  }
}
```

`files` 必须完整覆盖包内除 `manifest.json`、`signature.sig` 外的全部普通文件。App 安装时会重新计算哈希，任何缺失、多报或内容不一致都会拒绝安装。

### 15.3 包限制

```text
压缩包最大：10 MB
解压后最大：30 MB
单文件最大：10 MB
普通文件最多：500 个
```

禁止内容：

- 符号链接；
- 重复文件路径；
- 绝对路径；
- `.` 或 `..` 路径穿越；
- manifest 未声明的载荷文件；
- manifest 声明但包内不存在的文件。

`signature.sig` 是保留签名文件，不进入 `files` 哈希表。当前内部 Debug 体验包尚未完成正式签名链路。

## 16. App 安装和运行流程

App 选择 `.mmpkg` 后会：

```text
选择文件
  ↓
校验扩展名和包大小
  ↓
解析 ZIP
  ↓
校验 manifest、路径和资源限制
  ↓
重新计算全部文件 SHA-256
  ↓
解压到临时目录
  ↓
原子切换为正式安装版本
  ↓
注册插件并启动 WebView
```

安装目录由 App 管理，插件不能访问其他插件目录。已安装的同 ID 插件优先于 App 内置 Demo；卸载后自动回退内置版本。

当前仅支持 Debug 内部本地侧载。Release 版本仍需完成：

- 插件审核；
- 服务端签名；
- App 可信公钥验签；
- 插件市场下载；
- 在线升级与撤回；
- 正式权限授权页面；
- WebView 请求级网络域名隔离。

在这些边界完成前，未签名本地 `.mmpkg` 不能开放给普通 Release 用户。

## 17. CSP 和资源安全建议

插件应尽量使用严格的 Content Security Policy，例如：

```html
<meta
  http-equiv="Content-Security-Policy"
  content="default-src 'self';
           script-src 'self';
           style-src 'self';
           img-src 'self' data:;
           connect-src https://api.example.com;
           object-src 'none';
           frame-src 'none';
           worker-src 'none';
           base-uri 'none';
           form-action 'none'"
>
```

安全建议：

- 不使用 `eval` 或动态执行不可信脚本；
- 不从未审核 CDN 加载 JS；
- 尽量把 JS、CSS、字体和图片放入插件包；
- 网络请求只允许必要 HTTPS 域名；
- 不把 token、密码或私钥硬编码在 H5 中；
- 不信任设备事件或网络响应中的任意字符串；
- 对用户输入和远端内容进行转义；
- 不尝试访问 App 未授权的能力。

## 18. 从 ZIP 迁移到 npm

ZIP 是内部体验阶段的临时分发方式。正式 npm 包发布后，计划使用：

```sh
npm install @memomind/gm-plugin-web-sdk
npm install --save-dev @memomind/gm-plugin-studio
```

SDK import 从本地文件：

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
```

改为 npm 包：

```js
import { createGMPlugin } from '@memomind/gm-plugin-web-sdk';
```

Studio 从：

```sh
node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin ./dist
```

改为：

```sh
npx gm-plugin-studio --plugin ./dist
```

迁移到 npm 后不改变：

- `createGMPlugin()` 的业务调用方式；
- Bridge v1 方法和事件；
- 插件 manifest；
- `.mmpkg` 最终交付格式；
- App 的安装和权限校验逻辑。

## 19. 常见问题

### 19.1 页面一直显示“正在连接 App”

检查：

- 页面是否通过 Studio 或真实 App 打开；
- 是否直接双击使用 `file://` 打开了 HTML；
- SDK 文件是否成功加载；
- 浏览器控制台是否有 404、CSP 或模块解析错误；
- SDK import 是否使用了正确相对路径。

### 19.2 Studio 显示插件页面，但按键没有反应

检查是否先执行：

```js
await gm.device.subscribeEvents([
  'button',
  'imuGesture',
  'connection',
]);
```

并注册了对应监听器。

### 19.3 眼镜画面没有内容

手机端 HTML 不会自动同步到眼镜。需要显式调用：

```js
gm.display.updateText(...)
gm.display.updateImage(...)
gm.display.updateImageLz4(...)
```

同时检查设备连接状态和绘制区域是否超出 600×350。

### 19.4 Studio 文字和真机略有差异

Studio 使用浏览器 Canvas 模拟字体、换行和绿色亮度。固件字体、字形、光学亮度和真实刷新仍需真机验收。

### 19.5 `.mmpkg` 被 App 拒绝

检查：

- 文件扩展名是否为 `.mmpkg`；
- ZIP 根目录是否直接包含 `manifest.json`；
- `entry` 是否存在；
- `bridgeVersion` 是否为 `1.0`；
- 版本是否为语义化版本；
- 权限是否属于支持列表；
- 包、单文件、总解压大小和文件数量是否超限；
- 文件是否在生成哈希后又被修改。

不要手工修改已经生成的 `.mmpkg`，修改源码后应重新运行打包器。

## 20. 发布前自检清单

### 工程与 SDK

- [ ] 最终构建目录包含 `index.html`。
- [ ] 最终构建目录包含本地 SDK 或 npm 构建产物。
- [ ] 所有资源使用包内相对路径。
- [ ] 没有引用开发电脑绝对路径。
- [ ] `gm.ready()` 完成后才调用其他 API。

### manifest

- [ ] `id` 唯一且符合反向域名格式。
- [ ] `version` 是语义化版本。
- [ ] `entry` 指向实际 HTML 文件。
- [ ] `bridgeVersion` 为 `1.0`。
- [ ] 只声明实际需要的权限。

### 功能

- [ ] Studio 中插件页面正常运行。
- [ ] 眼镜画面未越界、未裁切。
- [ ] 单击、双击、长按按预期处理。
- [ ] 抬头、低头、左右转头按预期处理。
- [ ] 设备断连时不会无限重试。
- [ ] 生命周期暂停和恢复逻辑正常。
- [ ] 插件状态能正确保存和恢复。

### 安全与打包

- [ ] CSP 只允许必要资源和网络域名。
- [ ] 没有硬编码敏感 token、密码或私钥。
- [ ] 使用 DevKit 打包器生成 `.mmpkg`。
- [ ] 没有在打包后手工修改文件。
- [ ] `.mmpkg` 没有超过资源限制。
- [ ] 已在目标 App Debug 版本中完成安装和运行验证。

### 真机验收

- [ ] 字体、换行和布局已在真实眼镜确认。
- [ ] 绿色亮度和对比度可读。
- [ ] 图片传输和刷新耗时可接受。
- [ ] 快速操作不会导致画面破损或状态错乱。
- [ ] 已记录 App、插件、Bridge 和固件版本。
