# 运行模型与生命周期

同一份 Web 插件代码可以运行在两种宿主中：

- App WebView：H5 SDK 使用 `MemoPluginBridge` JavaScript channel。
- Studio：H5 SDK 使用 parent-frame `postMessage` transport。

宿主通过 session token 和 runtime generation 隔离旧页面。页面重载后，旧请求和旧事件不得进入新一代 runtime。

当前生命周期状态包括 `starting`、`running`、`suspended`、`stopped` 和 `failed`。插件应处理暂停、恢复、断连和 runtime 重建，不要假设页面永远常驻。
