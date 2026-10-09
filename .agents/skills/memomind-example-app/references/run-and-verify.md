# 运行、测试与排错

以下示例命令从仓库根执行；替换 `<name>` 为真实目录名后给开发者可直接执行的命令。

Windows PowerShell 将 `python3` 换成 `py`；包含空格的路径加引号。生成工具需要 Python 3.8+，Web 生成/刷新还需要 PhoneSDK 支持的 Node 18+，无需先安装 npm 依赖。

## Web 静态工程

```sh
node PhoneSDK/tools/run-browser-studio.mjs --plugin examples/<name> --port 4173
node PhoneSDK/tools/build-mmpkg.mjs PhoneSDK/examples/<name> PhoneSDK/dist/<name>.mmpkg
```

注意 launcher 的相对 `--plugin` 路径按 PhoneSDK 根解析，packager 的位置参数按命令的当前目录解析；不要把 launcher 参数误写成 `PhoneSDK/examples/<name>`。端口占用用 `--port 4174`，不要杀不相关进程。

打开 launcher 输出的地址，按当前程序需要的权限在宿主授权对话框中勾选并启动；必需权限未勾选时启动会被拒绝，这不是 Bridge 卡死。运行预览后检查手机页面、虚拟眼镜绘制、错误状态与重载。生成的最小 Web 工程有计数保存和文本同步按钮，不含复杂业务；以它验证 SDK ready、storage 和 display，再替换业务代码。

## Web Vite 工程

在新项目目录按其 lockfile 安装依赖，运行实际 package scripts。以 tic-tac-toe 风格为例：

```sh
npm ci
npm test
npm run build
```

然后回到仓库根：

```sh
node PhoneSDK/tools/run-browser-studio.mjs --plugin examples/<name>/dist
node PhoneSDK/tools/build-mmpkg.mjs PhoneSDK/examples/<name>/dist PhoneSDK/dist/<name>.mmpkg
```

Vite 的 public/manifest.json 与静态资产要进入 dist，资源使用相对路径。Vite 自带 dev server 不是 Bridge 宿主；不要仅在普通浏览器看 DOM 就宣布设备 API 已运行。

## Glass 工程

```sh
python3 GlassSDK/build.py build --example <name>
```

GMP 在 `GlassSDK/build-host/.build/<name>/<name>.gmp`，审核 sidecar 同目录。打开预编译 Desktop Studio、Import workspace 选择仓库根、刷新并选择程序。Studio 对集合目录下的一到三层插件支持扫描；更深目录或独立工程通过 Import package 导入，不以构建可递归推断 Studio 无深度限制。

使用本技能生成器的 `--output /tmp/...` 时，采用 `--project /tmp/...` 构建，输出在工程自己的 `.build/`；需要临时输出可通过 GlassSDK `--build-dir` 指定。不要在验证技能时重建并覆盖仓库的全部 GMP。

## 验证范围

业务算法、协议编码/解码和生命周期行为使用定向测试；公共 SDK 修改才运行 PhoneSDK `npm run verify`。新增 example 的行为不一定被根测试自动发现，查看 package.json 的 test glob，必要时明确执行新测试路径。

新 Glass 目录会被 `test_system_native.py` 扫描，它的 C mock 针对现有例子和 flags 编写。新程序若使用额外 Host 能力，核对或扩展 mock 的真实行为，不让通用扫例子测试被新程序破坏，也不能直接跳过新增例子。

对完成的程序至少核对启动、主要交互、存储恢复（若有）、拒权/不支持、停止/重载、断连与显示恢复（若相关）。布局变化查看手机窄屏和虚拟眼镜内容；光学、录音和 IMU 手感另列真机项目。

## 常见阻塞的定位

| 现象 | 优先核对 |
| --- | --- |
| Bridge 一直不 ready | 在 Studio/App 内启动；SDK 用 relative vendor 模块；没有直接打开 index.html 或只开 Vite server |
| 新程序刷新后看不到 | manifest 在正确目录，目录深度符合 Studio 扫描，构建成功且输出路径匹配；Vite 构建 dist |
| MMPKG 校验失败 | 打包最终目录，entry 存在，权限对象与 scope 合法，输出不在输入目录，未夹带 node_modules/源码工程 |
| API 拒绝或 CAPABILITY_UNAVAILABLE | 当前 capability、实际批准权限、scope、宿主支持与会话；用 permission-debug 核对，不改成永远成功 |
| H5 动了但眼镜不动 | 检查准确 GMP 配对、协议版本/ID、设备连接与发送失败；自定义消息不能只测 Browser Studio |
| 新 Glass 工程找不到头文件 | shared call_ui/素材路径和当前目录层级，独立工程 source-root/include-dir 边界 |
| 原生构建栈/RAM 超限 | 大局部数组、VLA、静态缓存、指针表；查看 .su 与链接段，不提高红线 |
| 录音只有电脑能用 | 确认 Host 音频接口与模拟源；不能拿电脑麦克风模拟结果当眼镜录音通过 |
