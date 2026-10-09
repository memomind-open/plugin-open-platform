# Plugin Open Platform AI 编程入口

先查看 `git status --short --branch`，读取根 README 和任务涉及的 SDK 文档。默认中文沟通，保留已有修改；提交、推送、发布按用户授权执行。此仓库是公开 SDK 分发仓库，不是官网前端或 Desktop Studio 源码仓库。

## 技能选择

项目技能位于 `.agents/skills/`，按任务读取对应 `SKILL.md`，不要一次加载全部文档。支持项目技能发现的工具可自动选择，也可用 `$技能名` 指定；其他 AI 工具直接读取下列文件。

| 用户任务 | 技能入口 |
| --- | --- |
| 从需求开发与 examples 类似的完整程序、复制/组合示例 | [memomind-example-app](.agents/skills/memomind-example-app/SKILL.md) |
| 新建或修改眼镜 C 插件、Host ABI、绘制、输入或生命周期 | [memomind-glass-plugin](.agents/skills/memomind-glass-plugin/SKILL.md) |
| 新建或修改 H5 插件、Bridge、权限、Browser Studio 或 Web SDK | [memomind-web-plugin](.agents/skills/memomind-web-plugin/SKILL.md) |
| 独立蓝牙客户端、HUD、录音、HOGP 或业务报文联调 | [memomind-business-protocol](.agents/skills/memomind-business-protocol/SKILL.md) |
| 构建、打包、审核附件、SDK 分发或交付检查 | [memomind-plugin-delivery](.agents/skills/memomind-plugin-delivery/SKILL.md) |

新应用先用 `memomind-example-app` 选型并完成工程，再按端组合相关技能。现有程序修复直接选对应技能。跨端任务组合读取相关技能。例如手机控制眼镜游戏：Web + Glass，涉及自定义报文时再读业务协议；需要交付包时再读交付技能。

## 工程边界与事实来源

- `GlassSDK/include/` 是 native ABI 的事实来源；`GlassSDK/docs/` 解释约束，`GlassSDK/examples/` 提供实现参考。
- `PhoneSDK/packages/bridge-contract`、`web-sdk`、`device-renderer`、`studio-runtime` 负责契约、公开 API、渲染、宿主模拟；产品逻辑放 examples，不反向塞进通用包。
- `Studio/` 是预编译分发产物。公开 Browser Studio 源码在 `PhoneSDK/tools/browser-studio`。修复 Desktop Studio 自身功能需要其源码，不能把改 SDK 当作已修复私有宿主。
- 以当前头文件、契约实现和测试核对文档。发现旧版本描述冲突时说明差异并修正任务涉及的文档，不照搬旧例子或另一个仓库的规则。
- 根构建用 Python；PhoneSDK 使用其 `package.json` 和 lockfile 的 npm 流程。这里没有官网仓库的 pnpm quality/harness 命令，不引入其路由、账号或分支角色门禁。
- SDK、示例与 Studio 的目录关系影响扫描和配对；保持根 README 所述布局。

## 实现与验收

动手前简要明确目标、具体修改范围、可观察的验收结果及要保留的行为，直接在对话中说明即可。根据任务选择定向测试或构建，不为纯文档、样式等小改动堆叠测试。

功能修复验证失败原因与修复后的行为；处理权限拒绝、不支持、断连、停止/重启等相关状态。不要删除断言、跳过失败或静态伪造成功来交付。

完成时检查 `git diff --check` 与实际改动范围。构建可能更新已跟踪的 `.gmp`、`.mmpkg`、审核附件或其他产物；确认它们属于交付范围，避免顺带更新全部示例和 Studio 二进制。原有用户产物不能直接覆盖或回退。

报告改动、执行的检查及结果、生成包位置和未验证项。自动测试、模拟器、真机和发布是不同证据；没有实际执行的步骤不能写成已完成。
