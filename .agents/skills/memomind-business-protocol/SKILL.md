---
name: memomind-business-protocol
description: 实现或核对 MemoMind 独立蓝牙客户端、GM 业务报文、HUD、音频和 HOGP 接入。用于公开业务协议与跨端消息联调，不用于可执行 GMP 安装传输协议。
---

# 公开业务协议

先区分请求使用原生蓝牙客户端、H5 Bridge 还是 GlassSDK Host API。它们的能力与权限不同，不能用某层文档推断另一层已经提供对应接口。

从 [Bluetooth developer guide](../../../GlassSDK/docs/BLUETOOTH_DEVELOPER_GUIDE.md) 入手，按任务读取：

| 任务 | 文档 |
| --- | --- |
| GM framing、TLV、长度和 checksum | [PROTOCOL](../../../GlassSDK/docs/PROTOCOL.md)、[WIRE_EXAMPLES](../../../GlassSDK/docs/WIRE_EXAMPLES.md) |
| 独立客户端完整 HOGP 流程 | [HOGP_QUICKSTART](../../../GlassSDK/docs/HOGP_QUICKSTART.md)、[BLE_ACCESSORY_PROTOCOL](../../../GlassSDK/docs/BLE_ACCESSORY_PROTOCOL.md) |
| HUD 文字、图形与位图 | [HUD_PROTOCOL](../../../GlassSDK/docs/HUD_PROTOCOL.md) |
| 麦克风 Opus / HFP 播放 | [AUDIO_PROTOCOL](../../../GlassSDK/docs/AUDIO_PROTOCOL.md) |
| 设备状态与显示控制 | [DEVICE_BUSINESS_PROTOCOL](../../../GlassSDK/docs/DEVICE_BUSINESS_PROTOCOL.md) |
| H5 与眼镜插件业务消息 | [application messaging](../../../PhoneSDK/docs/web-plugin/application-messaging.md)、[PROTOCOL_COMPATIBILITY](../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md) |

## 联调约束

BLE GATT、Classic SPP/iAP2、HFP/SCO 与眼镜到配件的 HOGP 链路分别处理。BLE 连接成功不证明 HFP 音频建立，也不证明固件开放了录音通道。

发现服务/特征和真实属性，不能硬编码 attribute handle 或混同文档中的 legacy UUID 表示。先订阅 uplink 再发送，按协商 MTU 与平台写入限制分片；ATT 写完成不是 GM 业务 ACK。

按实际协议核对大小端、长度含义、checksum、event ID、响应匹配和异步事件路由。断连时清理半包、pending 请求、HOGP handle 和录音会话；重连重新发现和订阅。

公开业务载荷、HUD 像素、音频与 HOGP 数据可以按文档实现；可执行 GMP 的安装元数据、传输块、恢复/激活事务不属于公开接入合同。安装使用官方 App，不从私有安装器推导新的公开接口。文档未公开不等于已验证硬件拒绝第三方客户端。

## 验证

使用 `GlassSDK/docs/examples/bluetooth_wire.py` 核对参考编解码接口，复算文档示例的每个字节、长度与 checksum。新增解析逻辑验证截断、错误长度、未知类型、分片和断连后的状态清理；利用已有测试框架，不凭空添加必须依赖真实设备的自动测试。

查阅 [PROTOCOL_COVERAGE](../../../GlassSDK/docs/PROTOCOL_COVERAGE.md) 的实现与缺口，注明目标固件版本和实际验证的链路。没有抓包或设备测试时，只报告编解码与模拟结果，不能声称 BLE/HFP 已端到端通过。
