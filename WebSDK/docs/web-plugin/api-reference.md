# Bridge v1 API 概览

## Runtime

- `runtime.ready`
- `runtime.ping`
- `runtime.getBridgeVersion`
- `runtime.getCapabilities`
- `runtime.getLifecycleState`

## Storage

- `storage.get`
- `storage.set`
- `storage.remove`
- `storage.clear`

## Display

- `display.createPage`
- `display.rebuildPage`
- `display.updateText`
- `display.updateImage`
- `display.updateImageLz4`
- `display.beginFrame`
- `display.updateFrameImageLz4`
- `display.closePage`

当前设备 profile 是 600×350、GRAY_4、30 Hz。文本、坐标和图片参数必须通过 SDK 校验；设备逻辑 ACK 不等于人眼已确认显示。

Scene Bridge 单次 payload 最大 81,901 B：Channel 6 原始 GRAY_4 使用 10 B 头，像素最多 81,891 B；Channel 7 raw LZ4 使用 14 B 头，压缩数据最多 81,887 B，并且解压后位图也不能超过 81,901 B。超限画面必须先拆成多个独立区块；LZ4 必须对每个区块分别压缩，不能压缩整屏后切割压缩数据。

需要一次性显示多个区块时，先调用 `display.beginFrame({ frameId, tileCount })`，再按 `tileIndex` 从 0 开始顺序调用 `display.updateFrameImageLz4`。宿主会在 Channel 8 Begin 和每个 Channel 9 Tile 后等待设备的 Channel `0x0104` 状态 ACK；中间 Tile 只写后台帧，最后一个 Tile 成功后才整体显示。传输失败后必须用新的 `frameId` 重建整帧，不能跳过失败分块。

## Device

- `device.getInfo`
- `device.subscribeEvents`
- `device.unsubscribeEvents`

事件包括 `device.button`、`device.imuGesture`、`device.rawImu` 和 `device.connection`。

`device.getInfo` 是无需权限的只读连接信息查询；订阅和取消订阅设备事件需要在 manifest
声明 `device.events`。

## Plugin Message

使用 `plugin.sendMessage` 向眼镜上当前运行的设备插件发送自定义二进制消息：

```js
const frame = Uint8Array.of(2, sequence, buttons >> 8, buttons & 0xff);
const result = await gm.plugin.sendMessage(0x4647, frame);
```

Receive a generic binary message sent by the currently running glasses plugin:

```js
const offMessage = gm.plugin.onMessage(({ channel, data }) => {
  if (channel !== 0x4648) return;
  console.log([...data]); // data is a Uint8Array
});

// Remove the listener when it is no longer needed.
offMessage();
```

The Bridge event name is `plugin.message`. Its wire data is
`{ channel, dataBase64 }`; `gm.plugin.onMessage()` validates the channel and
payload and exposes the decoded payload as `Uint8Array`. The event uses the
active `runtimeGeneration`, is not part of `device.subscribeEvents`, and does
not require a manifest permission.

An App host forwards an uplink by invoking the WebView callback with the same
event envelope:

```js
window.__memoPluginEmit({
  name: 'plugin.message',
  data: { channel, dataBase64 },
  runtimeGeneration,
});
```

- `channel` 必须是 `0..65535` 的整数。
- `data` 必须是非空 `Uint8Array`，最大 81,901 B。
- 此能力默认可用，不需要 manifest 权限。
- 返回成功表示设备已 ACK 且消息已送达当前运行的 GMP，不表示 GMP 的业务逻辑或显示结果已完成。
