> 2026-09-08 恢复受控配对通信：共 10 类权限、32 个 Bridge 方法。自定义 H5/设备配对插件声明 device.messaging 和 channels，授权后通过 gm.plugin.sendMessage/onMessage 双向通信；未授权、越界、失效运行态拒绝。标准显示/事件通道仍叠加相应权限。权限实验室只测试九类标准能力，不提供任意消息按钮。通信权限不代表设备插件内部敏感行为已被 App 隔离；真机配对验收待完成。

# Plugin Capability Lab 0.2.7

这是实际功能插件，不是日志展示页。宿主仍使用既有 Bridge 2.0，不升级 WebView。
在当前权限开发版 Desktop Studio 刷新手机插件列表，选择「Plugin Capability Lab · Bridge 2.0 0.2.7」。
显示测试配对 GM Web Bridge；不提供通用消息发送功能。

九种权限都声明为 optional，方便启动时全拒绝、部分同意、全部同意，比较真实操作结果。
宿主未开放的能力继续走真实接口并展示拒绝原因；H5 不调用电脑麦克风代替眼镜采集。

| 权限 | 可实际操作 |
| --- | --- |
| storage | 便签保存、读取、删除单个专用 key |
| files.user-selected | 宿主选文件、文件列表、二进制流读取、文本/图片/音频预览、单文件删除确认 |
| display | 编辑文字并发送眼镜、发送 GRAY_4 棋盘、关闭页面 |
| device.info | 手动读取设备并展示字段 |
| device.events | 订阅/取消，按键计数、头动指示点、连接变化 |
| audio.capture | 眼镜原生 recording 模式采集、停止、采集状态、H5 接收 Opus 数据及帧边界 |
| audio.playback | 本地生成 WAV 测试音、延迟 play()、H5 播放收到的眼镜 Opus 录音 |
| network | 预填支持 CORS 的地址，可修改后实际 H5 GET、状态码/耗时/响应正文、超时/取消 |
| location.foreground | 原生单次/监听/取消，坐标、精度、采样时间及离线轨迹图 |

所有文件预览限 1 MiB、网络正文限 64 KiB；位置和文件不上传。
测试音为 2 秒低幅度 PCM WAV。锁屏/页面隐藏时保留正在播放的音频和录音会话，但取消网络、定位和事件订阅；真正卸载页面时才全量清理。
不保存权限审批结果，不使用 H5 localStorage 替代 storage Bridge。

## 0.2.7 变更

- 插件清单名称、页面标题和主标题统一改为英文 `Plugin Capability Lab`。

## 0.2.6 修复

- 锁屏、`document.hidden`、`pagehide` 和 Runtime `suspended` 不再主动暂停 H5 音频或调用 `audio.stopCapture`。
- 锁屏时仍取消网络请求、定位和设备事件订阅；`beforeunload` 保留完整释放。

## 0.2.5 修复

- 展示录音帧数、Opus 字节数和采集时长；这些指标不等于有效人声。
- 原生回放期间禁用重复播放和测试音按钮，终态恢复；显示并记录完整播放错误。
- 修正 capturing 状态识别；当时页面隐藏会停止采集，已由 0.2.6 调整。
- 网络默认使用支持 CORS 的公共测试接口，不代理请求、不绕过跨域限制。
- App 导出日志同步保留录音数值统计及哈希后的回放 ID，不保存音频内容。

## 直接运行

在 PhoneSDK 目录：

```sh
npm run dev:permissions -- --port 4187
npm run pack:plugin -- examples/permission-debug dist/permission-debug-0.2.7.mmpkg
```

若端口上已有此工作区的预览，直接刷新即可。
Browser Studio 的 Reload 保留当前打开工作区的便签和文件，但每次重新申请授权；关闭或整体刷新 Browser Studio 页面仍是内存模拟器，数据不持久化。Desktop 存储由其宿主管理。

## 测试建议

1. 全部拒绝启动：Bridge 各功能应显示 NOT_GRANTED。再重载并勾选目标权限。
2. 写便签，清空输入框，读取后应恢复；用宿主 Reload 后再次授权读取。
3. 选择附带 sample-note.txt，读取预览应显示实际中文内容。
4. 发文字和棋盘，观察眼镜/虚拟显示；仅看插件小图不算发送成功。
5. 监听后按眼镜按键 / Studio Single click，计数应增加；模拟头动后指示点移动。
6. 播放测试音；输入自己允许访问的测试 URL 发送 GET。
7. 负向测试在折叠区：rawImu 范围、未知 network.request 方法。

## 不混淆的边界

- Studio 定位来自宿主模拟器，页面醒目标注“模拟位置”；App 真机则展示 App 原生返回。没有偷偷使用 navigator.geolocation 绕过 Bridge。
- macOS Desktop Studio 使用电脑麦克风模拟眼镜 Opus 采集并将数据交给 H5 播放；真实眼镜录音仍需 App + 眼镜验证。
- H5 播放与 HTTP 是真实操作，但 Studio 没有原生隔离。拒绝权限后仍能播放/请求，意味着该宿主没有拦截，不能宣称验收通过。
- GET 失败可能是 CORS/网络故障，不能自动判成权限拦截。默认 https://httpbin.org/get（第三方公共服务，可能不可达）；仅点击后请求，不自动联系外部服务。
- 自动播放探针曾有页面交互，不能替代冷启动无用户手势的真机测试。
- App 完整插件 Runtime 的开放限制沿用既有结论，本插件不会绕过它。

2026-09-07 页面实测：便签存取、文字/棋盘发送、按键计数、头动反馈、设备信息、测试音 play()、HTTP 200、本地文件流预览成功；
位置明确显示模拟来源，录音真实返回 CAPABILITY_UNAVAILABLE。该历史实测不代表当前版本已通过真机验收。
