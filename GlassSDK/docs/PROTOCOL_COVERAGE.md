# 业务通信协议文档覆盖检查（2026-09-17）

## 开放边界

公开插件业务消息及其他设备功能的协议格式。插件包安装、压缩分块、续传等
由官方 APP 管理，不在公开文档提供可独立实现安装器的指令和报文说明。

“有源码接口”不等于“已经形成完整的公开契约”。以下检查以当前接口注册表、
处理函数和现有 Markdown 为依据，不把枚举中的名称当成已验证的完整协议。

## 本轮已补齐或纠正

| 项目 | 发现 | 处理 |
| --- | --- | --- |
| 双向自定义消息 | API 页写未开放，开发指南却写无需权限，与 APP 实现冲突 | 统一为 `device.messaging` 授权和通道范围控制 |
| 收包权限 | 仅说明监听回调，未讲清入站也按通道过滤 | 补充双向授权、标准显示/事件通道额外权限和失效运行代次过滤 |
| 二进制与 Bridge 的关系 | Base64、业务字节、GM 帧容易混淆 | 补充 H5 Uint8Array、Bridge Base64、设备二进制三个层次 |
| 应答语义 | 容易把命令 ACK 当业务处理完成 | 补充独立业务回复、请求 ID、超时和重复处理责任 |
| 错误码 | 插件 INT8 status、系统 STATUS 和 Bridge 字符串错误缺少区分 | 补充插件数值错误表及 Bridge 权限/运行态/参数错误 |
| 承载名称 | SPP UUID 容易被当成 BLE GATT characteristic | 明确 Classic SPP、iAP2 和 BLE/HOGP 是不同通道 |
| 设备信息查询 | 文档写无需权限 | 修正为 `device.info` |
| 权限 manifest | 保留旧字符串列表、Bridge 1.0 示例 | 包格式页更新为当前对象声明和版本要求 |

参见 [业务协议](PROTOCOL.md)、
[APP 插件双向通信](../../PhoneSDK/docs/web-plugin/application-messaging.md)。

## 后续补齐结果

| 接口族 | 已补内容 | 文档 |
| --- | --- | --- |
| BLE/HOGP 戒指网关 | 控制/GATT/HID 指令与 TLV、接收上限、ACK/异步结果、句柄代际、扫描及发现分片、错误处理；纠正旧设计中 ACK 带 JSON/seq 和 unbond 同步完成的说法 | [BLE accessory](BLE_ACCESSORY_PROTOCOL.md) |
| 系统状态与显示 | 信息/状态查询的 STRING JSON 格式、常用字段、亮度/高度/距离取值与异步应用限制 | [Device business](DEVICE_BUSINESS_PROTOCOL.md) |
| 翻译、提词器、通知、导航 | 常用消息字段/顺序、上下行区别、旧通知指令停用、导航实际模式与能力限制 | [Device business](DEVICE_BUSINESS_PROTOCOL.md) |
| 通用帧 | 两帧可重建二进制例子、逐帧校验和、逻辑/物理长度区别、跨帧 TLV、断线/错误重置与重试语义 | [GM protocol](PROTOCOL.md#fragmentation-and-recovery) |
| 定位接口 | 三个方法、两个事件、WGS84 字段、前台生命周期、超时、错误和示例 | [Foreground location](../../PhoneSDK/docs/web-plugin/location.md) |
| APP 权限与版本 | 32 个方法与权限逐项映射、10 类权限、scope、当前对象声明及 Bridge/schema 版本 | [Capability contract](../../PhoneSDK/docs/web-plugin/capability-contract.md) |
| 音频路径 | 修正 glasses Host 能力矩阵中“录音只能 HFP”的误导；PhoneSDK 原生 Opus 流和 HFP 通话分开 | [Capability matrix](CAPABILITY_MATRIX.md) |

## 覆盖范围与验证

以上是依据当前源码补充的接口契约，不承诺将固件所有内部调试/工厂命令公开为
稳定 SDK。产品特有的完整导航图像/地图对象、出租车业务、HID 输入规则表等复杂
业务模型，仍需独立的产品/附件契约，不能用通用 BYTES 或一个指令枚举替代其定义。
本文明确标出这一范围，未把这些模型宣称为已完整文档化。

核对依据为 APP `plugin_location_adapter/source.dart`、`plugin_capability_policy.dart`、
`common_blue/lib/command` 中的业务编码器，SDK `permission-policy.js`、`web-sdk`，以及
固件 `gm_package.c`、`bt_data_handler.c`、`display_main_event.c`、`navi_app.c` 和
`app_hogp_relay.c`。处理函数优先于历史设计和过期注释。

权限策略、Studio 权限、Web SDK 的 62 项已有测试通过。测试时用临时 Node loader
解析本地 workspace 包，避免依赖未链接的问题；未修改仓库源码或依赖文件。
二进制示例已计算长度和校验和；这不替代真实蓝牙链路时序验收。

## “只能官方 APP 安装”的保证范围

APP 内的权限和通道控制用于约束 H5 插件，不等同于眼镜端验证安装客户端身份。
本次检查的安装命令分发路径没有展示独立的官方 APP 身份验证；这不是对全部
蓝牙栈鉴权路径的完整安全审计。隐藏文档不能作为“非官方客户端一定无法安装”的证据。

如果产品要求强制只接受官方授权安装，需要单独核对眼镜端是否验证可信的安装授权，
并覆盖新安装、续传和缓存激活的授权生命周期。该项属于实现与安全设计，本轮未改代码。
