# Web 插件开发文档（Draft）

本目录是 GM Web 插件公开开发文档的首版骨架。当前契约版本为 Bridge v1。

## 阅读顺序

1. [快速开始](quick-start.md)
2. [压缩包 DevKit 使用说明](devkit-zip.md)
3. [运行模型与生命周期](runtime-and-lifecycle.md)
4. [API 概览](api-reference.md)
5. [Studio 调试](studio.md)
6. [最终插件包 `.mmpkg`](package-format.md)
7. [通过局域网安装 `.mmpkg`](lan-install.md)
8. [兼容性与真机边界](compatibility.md)

Web 插件的 HTML 页面运行在手机 App 或 Studio 中；只有通过
`gm.display.*` 提交的内容才进入眼镜显示画面。
