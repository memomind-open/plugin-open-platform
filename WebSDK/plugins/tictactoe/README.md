# Aphrodite 中文井字棋插件

独立运行的 TypeScript/Vite Web 插件，通过 Aphrodite Bridge v1 与 App 通信。插件不直接访问蓝牙；设备绘制和按钮/IMU 事件均由 App 转接。

## 玩法

- 玩家执 X，电脑执 O，玩家先手。
- 抬头、低头在同一列向上、向下选择空格。
- 向左、向右转头在同一行选择空格。
- 单击主按钮落子，双击主按钮重新开始。
- 每局结束后，网页和设备会明确提示“你赢了”“你输了”或“打平了”。
- 网页上的按钮和棋盘也可以直接操作。

## 设备画面

- 设备按 576×288 横屏布局绘制。
- 左侧为中文规则、操作说明和当前状态。
- 右侧为 256×256 的 GRAY_4 位图棋盘，包含 X/O、当前选中框和获胜连线。
- 左侧中文规则先更新，右侧 256×256 棋盘作为最后一次绘制提交，避免后续文本覆盖棋盘刷新。
- Web 端将整张 GRAY_4 棋盘编码为严格 raw LZ4 block，通过 `display.updateImageLz4` 交给 App；压缩无收益时回退为一张完整的未压缩图，不再拆成四块，避免部分分块成功造成破图。
- 高频操作采用 latest-wins：只保留一张在途棋盘和一张最新待发送棋盘，避免旧帧在蓝牙队列中堆积并连续超时。

## 开发

```bash
# 在 plugins/tictactoe 目录执行
npm ci
npm test
npm run typecheck
npm run build
```

构建产物位于 `dist/`。`vite.config.ts` 使用相对资源路径，产物可以由 Aphrodite 的本地插件资源服务器直接加载。

在 WebSDK Browser Studio 中验证构建产物：

```bash
# 在 WebSDK 工作区根目录执行
node tools/studio-cli.mjs --plugin plugins/tictactoe/dist
```

打包：

```bash
npm run pack:plugin -- plugins/tictactoe/dist dist/tictactoe.mmpkg
```

## App 接入

生产构建产物需要完整同步到 Aphrodite 的 `assets/plugin_tictactoe/`。源码、测试、Node 依赖和 `node_modules` 不进入 App 仓库。

设备真机运行的前提是已安装支持 Scene channel、Ping、按钮和 IMU 事件的统一设备插件。
