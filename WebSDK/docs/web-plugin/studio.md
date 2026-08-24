# Studio 调试

Studio 同时展示 Web 页面和虚拟眼镜屏幕，并允许注入：

- 单击、双击、长按；
- 抬头、低头、向左转头、向右转头；
- 连接/断开；
- running/suspended 生命周期。

Studio 会显示 Bridge 请求日志。模拟错误和延迟属于后续切片；插件当前仍应处理 SDK 暴露的结构化错误。

Studio 同时模拟 Scene Bridge 的真实传输限制：单次 payload 最大 81,901 B。Channel 6 原始图片、Channel 7 raw LZ4 或 Channel 9 原子帧 raw LZ4 的压缩/解压尺寸超限时，请求返回 `PAYLOAD_TOO_LARGE` 且不会绘制。Channel 8 开始原子帧后，中间分块只写入后台帧，最后一块才刷新设备预览。设备预览上方会显示最近一次绘图使用的通道和字节数；超限时提示插件按区块拆分，并对每个 LZ4 区块独立压缩。
