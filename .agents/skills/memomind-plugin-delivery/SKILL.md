---
name: memomind-plugin-delivery
description: 构建、打包和检查 MemoMind Web/Glass 插件、审核附件、DevKit 与公开 SDK 分发产物。用于 .mmpkg/.gmp 或 Studio 交付准备，不包含未授权提交、发布或私有 Desktop Studio 编译。
---

# 构建与交付

先明确交付的是 Web `.mmpkg`、Glass `.gmp`、DevKit ZIP 还是 Studio 导出的组合应用 ZIP；核对源文件、manifest、版本与用户要求的范围。文档路径相对本文件，构建命令的目录如下。

新程序的源目录、dist、预览参数和常见错误见 [应用运行与验证](../memomind-example-app/references/run-and-verify.md)。PhoneSDK `build.py` 没有单项目筛选参数；新程序交付优先直接构建该工程并调用 packager，避免重写无关分发包。

## 构建入口

- 仓库根：`python3 build.py --help` 查看参数，`python3 build.py web` / `python3 build.py glass` 选择端；只有需要全量交付时才使用全量构建。
- `GlassSDK/`：`python3 build.py build --example <relative-path>` 构建单插件；独立工程使用 `--project`，细节见 [GlassSDK README](../../../GlassSDK/README.md)。首次构建可能下载锁定工具链，不替换其版本/校验来绕过问题。
- `PhoneSDK/`：`python3 build.py --help` 查看发现、构建及输出参数。单个 H5 工程先运行自己的构建，再用 `npm run pack:plugin -- /absolute/plugin/dist /absolute/release/plugin.mmpkg` 打包，输出不能位于输入目录中。静态示例的输入应是其实际部署目录。

## 包与审核附件

Web 打包规则见 [package format](../../../PhoneSDK/docs/web-plugin/package-format.md)：manifest、entry、权限和文件哈希须与最终输出一致；不手改压缩包、hash 或 signature 来掩盖验证失败。

Glass 构建生成 `.gmp`、`.review.json`、`.review-source.enc`，必须保持同次构建的一组产物。读 [REVIEW_PACKAGES](../../../GlassSDK/docs/REVIEW_PACKAGES.md)：Studio 使用已生成附件，不重新收集当前工作区；修改源码/依赖后先重新构建。外部依赖须满足 SDK/toolchain/source root 的收集边界，不静默漏掉构建输入。

公开分发使用公钥加密审核源码，不复制私钥或内部审核工具到公开仓库。加密、CRC、hash 或重建通过不等于审核通过、签名可信或 native 内存沙箱。

DevKit 见 [devkit ZIP](../../../PhoneSDK/docs/web-plugin/devkit-zip.md)，对应命令为 `PhoneSDK/` 的 `npm run build:devkit`。检查实际 ZIP 内容及引用文件，不顺带修改发布版本或其他包。

## 预览与交付证据

读 [Studio README](../../../Studio/README.md)、[INSTALLATION](../../../GlassSDK/docs/INSTALLATION.md) 与 [LAN install](../../../PhoneSDK/docs/web-plugin/lan-install.md)。Desktop Studio 可导入 GMP/MMPKG/组合 ZIP，Browser Studio 仅模拟 Web Bridge。保持 SDK 与 Studio 布局及插件扫描深度约定。

构建会写入部分已跟踪分发目录。构建前后比较 git status，核对本次生成的包和附件，不覆盖用户已有改动，不顺带替换 Studio EXE/安装包。需要私有 Desktop Studio 源码才能完成的工作，说明缺失依赖和可完成部分。

按任务执行定向测试；公共 PhoneSDK 变更用 `npm run verify`，Glass 打包/ABI 改动选择相关 `tests/`。仅编辑技能文档时验证技能结构、文档链接和 diff 即可，不全量重建 SDK。

最终给出包路径、对应源版本/工作区状态、已执行检查、模拟器/真机结果及待验证项。未部署不写已发布，未真机验证不写硬件通过；签名、线上送审和市场分发支持以当前文档与实现为准，不从规划推断已实现。
