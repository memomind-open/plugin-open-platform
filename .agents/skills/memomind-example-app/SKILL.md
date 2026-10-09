---
name: memomind-example-app
description: 根据开发者需求，从 MemoMind examples 开始完成可运行的新插件或双端应用，包括示例选型、最小工程、业务实现、配对、预览和打包。用于计时器、游戏、阅读器、桌面、宠物等类似示例的程序开发。
---

# 从需求到可运行的示例应用

此技能负责应用开发全程。先读仓库根 `AGENTS.md`，根据 [示例选型与开发路线](references/example-recipes.md) 选实现方式，再按实际涉及的端读取 [Glass 技能](../memomind-glass-plugin/SKILL.md) / [Web 技能](../memomind-web-plugin/SKILL.md)；普通 H5 插件不需要先学习原始 GM 蓝牙封包。

开发者可直接使用 [需求示例](references/developer-prompts.md) 中的计时器、小游戏、遥控和阅读器请求。

## 开始与默认选择

把需求整理成应用位置（手机/眼镜/双端）、主要交互、数据来源、持久化需求和可观察验收。用户未指定时，优先沿用最接近的示例架构，说明选择并直接实现；只有会改变核心体验或需要缺失的外部接口时再询问。

- 眼镜独立运行、本地游戏或原生高效绘制：Glass 插件。
- 手机拥有状态，眼镜显示文字/Canvas 与输入：Web 插件 + 现有 `web_bridge`，通常无需新写 GMP。
- 两端有专用状态/流协议：Web + 专用 Glass 配对，先定义消息再实现。
- 仅手机功能：Web 插件，删去不需要的 deviceRequirements 和权限，不凭空要求眼镜。

开发新程序时创建新目录与独立 ID，不覆盖参考示例；修改现有 example 的请求按原目录实现，不强制复制。最小工程使用本技能工具：从仓库根运行

```sh
python3 .agents/skills/memomind-example-app/scripts/create_example.py \
  --kind web --name focus-timer --id com.example.focus-timer --title "Focus Timer"
# 原生 LVGL 最小工程：把 web 改为 glass，使用独立目录名和 ID。
```

工具拒绝覆盖已有目录；Web 工程捆绑当前本地 SDK，Glass 工程使用当前 lvgl_ui 并复制电话 UI 共享头文件。它只生成可运行起点，**不会生成需求中的完整业务**。需要复杂例子的资产/协议时按路线读取并移植，不把完整巨型例子当空白模板。

## 完成实现

1. 先让最小页面/原生 UI 跑通一次，再增加状态、交互和真实数据。沿用当前公开 API，不重新手写 Bridge handshake，不把显示在手机 DOM 的效果算成眼镜显示完成。
2. 新身份更新 manifest、页面标题、状态存储键及必要的包元数据。复制例子时检查相对 include/import、vendored SDK、资源路径与设备配对 ID；不要全局替换原例子字符串或虚报仍未实现的协议能力。
3. 跨端开发按 [双端协议与联调](references/paired-app.md) 同时核对双方；标准 display/device API 沿用已有 Web Bridge 协议。
4. 移植所需功能，删掉新工程中不用的权限、远端域名、音频/图像资产和 UI；保留源示例本身。网络与存储失败显示真实错误，用户取消、拒权和断连可恢复。不要把演示数据写成真实结果。
5. 业务逻辑先做定向验证，再预览和打包；用户要求做程序时交付实现与可运行包，不停在设计或提示词。不要为了新应用顺带修改 SDK ABI、私有 Studio 或其他示例。

生成器回归检查：从仓库根运行 `python3 .agents/skills/memomind-example-app/scripts/test_create_example.py`，覆盖真实 Web 打包、覆盖拒绝、SDK 单项目刷新与 Glass 头文件可编译性；它不替代应用业务验收。

## 验收与交接

按 [运行、测试与排错](references/run-and-verify.md) 使用对应目录命令，区分源目录与构建 dist、Browser Studio 与 Desktop Studio。检查 manifest 与模块/资源都在最终包内。

给开发者新目录、启动命令、应选择的 GMP（如有）、包路径、已检查结果与待真机项目。附一段该应用如何继续改动的简短说明，例如业务状态、绘制、协议各在哪个文件；不要求开发者重新读完整 SDK 手册才能启动。
