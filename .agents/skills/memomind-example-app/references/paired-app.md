# 双端协议与联调

先检查是否能用标准 `gm.display`、`gm.device` 完成；若能，就配对现有 [web_bridge](../../../../GlassSDK/examples/web_bridge/)，不新增应用 channel、不重写安装流程。

专用双端应用先形成一张双方一致的消息表：方向、channel、版本、长度、字段类型与大小端、返回/ACK、重试及状态复位。参考 [fighter-controller/protocol.js](../../../../PhoneSDK/examples/fighter-controller/protocol.js)、[novel_reader README](../../../../GlassSDK/examples/novel_reader/README.md)，只借用符合任务的语义。

## Manifest 与配对

- 新 Web/Glass 程序用独立 package ID；允许两端使用同一业务 ID，但目录/版本格式仍各自遵守 SDK。
- Web 的 `deviceRequirements` 与 Glass 的 `provides.protocols` 对齐。仅当业务必须固定 GMP 时设 `requiredPluginId`；兼容的显示实现优先协议约束与 `preferredPluginId`。
- 如果仍使用旧协议，保持实际兼容并沿用协议 ID；若载荷不兼容，定义新的协议 ID/版本并同时更新要求与提供端。不能改 manifest 宣称兼容但只改一端编码。
- Web `device.messaging.scope.channels` 包含实际发送和接收的自定义通道；`device.events.scope.types` 仅列实际订阅事件。注册 listener 不等于授予权限，声明不等于运行时批准。
- 参考 [PROTOCOL_COMPATIBILITY](../../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md) 核对协议匹配；单独导入 GMP 时保留旁边 manifest，否则 Studio 可能只能报告未知兼容性。

## 数据与失败状态

接收端先校验版本、长度、范围再解码；发送端不能把 Bridge/BT 接受当应用已执行。需要可靠状态时定义业务 ACK、超时、重复请求处理与有界重试，不在显示回调忙等。

控制器释放触摸、窗口失焦或连接丢失时清掉按键状态；GMP 侧按协议检测过期控制包并回到安全输入，不能永久保持“按下”。连不上时显示离线，重连同步完整必要状态。

画面走 Web Bridge 标准接口时不手写整个 GM envelope；原子帧按 begin/tile 状态 ACK 串行传输。电话 UI 期间继续接收/ACK，抑制绘制，结束后重新发送需要恢复的静态画面。

## 最小联调顺序

先各自验证状态机和编解码，再在 Desktop Studio 选中准确的一对程序，验证一个按钮→一个状态变化→一个响应。通过后增加连续输入、大数据、断连/重连和错误报文。Browser Studio 的成功不能证明自定义 GMP 已收到了数据。

最后导出所选组合 ZIP 并在支持的 App/固件上验证；缺少设备时交付本地包和清晰的未验证项目，不能写成真机已通过。
