# 最终插件包 `.mmpkg`

Web 插件交付给 App 时必须打包为 `.mmpkg`。`.mmpkg v1` 是 ZIP 容器，插件文件直接位于压缩包根目录，不能再额外套一层项目目录。

```text
example-1.0.0.mmpkg
├── manifest.json
├── index.html
└── assets/
    ├── index.js
    └── index.css
```

`.mmpkg` 与安装到眼镜设备的 `.gmp` 不是同一种格式：`.mmpkg` 由手机 App 安装并在 WebView 中运行；`.gmp` 属于眼镜端原生插件。

## 构建前的 manifest

H5 构建输出目录根部必须有 `manifest.json`。开发者维护业务字段，`schemaVersion` 和 `files` 由打包器生成：

```json
{
  "id": "com.example.weather",
  "name": "天气插件",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": ["display", "device.events", "storage"]
}
```

字段约束：

- `id`：反向域名形式，最大 128 字符，例如 `com.example.weather`。
- `name`：非空展示名称，最大 80 字符。
- `version`：语义化版本，例如 `1.0.0` 或 `1.0.0-beta.1`。
- `entry`：包内相对 HTML 路径，最大 256 字符；不能包含空路径段、`.`、`..`、反斜杠或绝对路径。
- `bridgeVersion`：当前固定为 `1.0`。
- `permissions`：最多 16 项，不能重复；当前只允许 `display`、`device.events`、`storage`、`network`。

权限含义：

| 权限 | 能力 |
| --- | --- |
| `display` | 创建、更新和关闭眼镜显示页面 |
| `device.events` | 订阅按钮、头部动作、连接状态及 IMU 事件；只读 `device.getInfo` 不需要此权限 |
| `storage` | 使用当前插件隔离的 App 键值存储 |
| `network` | 声明插件需要网络；当前 Debug App 尚未按域名执行网络沙箱 |

App 会在 Bridge 调用时检查 `display`、`device.events` 和 `storage`。插件不应声明未使用的权限。使用网络的插件仍需配置严格的 CSP；不能把 `network` 声明视为 App 已完成网络隔离。

`plugin.sendMessage` 使用 App 已安装并启动的当前设备插件，默认不需要 manifest 权限；
它不会替插件安装或选择 `.gmp`，也不会绕过设备连接状态和协议 ACK。

## 设备插件依赖

Web 插件可通过 `deviceRequirements` 声明所需设备协议，供 Studio 在运行前自动配对和
检查兼容性：

```json
{
  "deviceRequirements": {
    "preferredPluginId": "com.gm.example.web-bridge",
    "protocols": [
      { "id": "gm.scene", "minVersion": "1.0" },
      { "id": "gm.device-events", "minVersion": "1.0" }
    ]
  }
}
```

- `protocols`：必需协议列表，最多 16 项；设备端提供的版本必须不低于 `minVersion`。
- `preferredPluginId`：协议兼容时优先自动选择的设备插件，不是强制 ID 绑定。
- `requiredPluginId`：严格要求的设备插件 ID，仅用于不能被兼容实现替换的配对。
- `minPluginVersion`：`requiredPluginId` 的最低数字版本。

协议 ID 使用小写字母、数字、点和连字符；版本为一到四段数字。设备 `.gmp` manifest
通过 `provides.protocols` 声明提供的协议。公共协议及设备端格式见
[`../../../GMPluginSDK/PROTOCOL_COMPATIBILITY.md`](../../../GMPluginSDK/PROTOCOL_COMPATIBILITY.md)。

## 打包命令

先用 H5 工具链生成可部署目录，再运行 SDK 仓库提供的打包器：

```sh
npm run build

node /path/to/GMWebPluginSDK/tools/build-mmpkg.mjs \
  ./dist \
  ./release/weather-1.0.0.mmpkg
```

在本 SDK 仓库中也可以使用：

```sh
npm run pack:plugin -- \
  /absolute/path/to/plugin/dist \
  /absolute/path/to/release/plugin.mmpkg
```

输入目录必须是最终 H5 产物，而不是包含 `src`、测试、`node_modules` 的工程根目录。输出文件不能放在输入目录内部，扩展名必须为 `.mmpkg`。

打包器会：

1. 校验 manifest、入口文件、权限和路径。
2. 拒绝符号链接及超限文件。
3. 对除 `manifest.json`、`signature.sig` 外的全部普通文件计算 SHA-256。
4. 写入 `schemaVersion: 1` 和完整 `files` 哈希表。
5. 生成确定性 ZIP 结构并原子写出 `.mmpkg`。

最终包内 manifest 类似：

```json
{
  "schemaVersion": 1,
  "id": "com.example.weather",
  "name": "天气插件",
  "version": "1.0.0",
  "entry": "index.html",
  "bridgeVersion": "1.0",
  "permissions": ["display", "device.events", "storage"],
  "files": {
    "assets/index.css": "sha256:<64 位小写十六进制>",
    "assets/index.js": "sha256:<64 位小写十六进制>",
    "index.html": "sha256:<64 位小写十六进制>"
  }
}
```

`files` 必须完整覆盖包内载荷文件，不能缺失或多报。App 安装时会重新计算哈希，任何内容不一致都会拒绝安装。

## 包限制和安全边界

- `.mmpkg` 最大 10 MB。
- 解压后总大小最大 30 MB。
- 单文件最大 10 MB。
- 包内普通文件最多 500 个。
- 禁止符号链接、重复路径、绝对路径和路径穿越。
- `signature.sig` 是保留签名文件，不进入 `files` 哈希表。

当前 App 只在 Debug 环境支持未签名本地侧载。Release 的可信公钥、审核签名、插件市场、在线升级和撤回尚未完成；未签名包不能作为正式 Release 分发方式。

## 安装后的 App 行为

App 选择 `.mmpkg` 后会校验 ZIP、manifest、资源限制和全部文件哈希，随后原子安装到 App 私有目录。相同 `id` 和 `version` 会替换该安装版本；已安装的同 ID 插件优先于 App 内置 Demo，卸载后回退到内置版本。
