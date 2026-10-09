---
name: memomind-glass-plugin
description: 开发或修复 MemoMind GlassSDK 原生 C 插件、Host ABI、显示、输入与生命周期。用于眼镜端 .gmp 实现，不用于独立蓝牙客户端或 Desktop Studio 私有源码修改。
---

# 眼镜原生插件

下列路径相对本文件；命令注明执行目录。首先阅读 [GlassSDK 指南](../../../GlassSDK/README.md)、[ABI](../../../GlassSDK/docs/ABI.md)，并核对 `GlassSDK/include/` 中任务涉及的公开头文件。按功能补读文档，不复制一份 ABI 手册到技能里。

## 选择入口

- 绘制：读 [GRAPHICS](../../../GlassSDK/docs/GRAPHICS.md)，参考 framebuffer / lvgl_ui / image_animation。
- 输入与 IMU：读 [INPUT](../../../GlassSDK/docs/INPUT.md)，参考 input / imu。
- 蓝牙消息：参考 bluetooth / web_bridge 及 [协议兼容](../../../GlassSDK/docs/PROTOCOL_COMPATIBILITY.md)。跨端字段、长度、通道与版本要同时核对发送端和接收端。
- 能力或扩展：读 [能力矩阵](../../../GlassSDK/docs/CAPABILITY_MATRIX.md)，检查 ABI、表长度、版本和实际函数指针；manifest 不代表固件能力。
- 显示插件：读 [SYSTEM_EVENTS](../../../GlassSDK/docs/SYSTEM_EVENTS.md)，参考 `examples/common/call_ui.h` 和 fighter_arena 的直接 framebuffer 策略。

## 新程序起点

应用需求先按 [应用开发技能](../memomind-example-app/SKILL.md) 选择示例或生成最小工程。显示尺寸通过 `host->display_get_info()` 查询，不把 Studio 的 600×350 当所有固件的固定能力。

复制示例时给新程序独立 manifest 身份，检查 common/call_ui.h 和跨例子素材 include；目录深度改变时修正相对路径。最小生成器把 call_ui.h 放入新工程，方便独立构建。更复杂的图片、IMU、游戏状态沿用对应示例的实际 API 与资源格式。

## 实现中保留的约束

只导出 `gm_plugin_entry`，通过 Host 表访问固件服务；入口验证并填描述符，不分配资源。`on_load` 不创建 UI；`on_start` 开始可见周期。失败的 load/start 自行释放部分资源，不能依赖未被调用的 unload/stop。

回调运行在显示任务，不能阻塞、sleep 或忙等；借用事件数据只在回调内有效。插件不能删除 Host 的 LVGL root。IMU raw 数据按公开 pull 接口读取。

电话 UI 出现时，显示插件应在所有绘制路径让出像素，包括按键触发绘制；保持 loop、蓝牙、输入及协议 ACK 工作。恢复时安排下一轮绘制，旧固件缺扩展时提供合理降级，不能声称当前 Studio 能模拟该事件。

保持冻结 core ABI；新增不兼容扩展使用独立扩展 ID，不能随意修改已发布表布局。不要直接链接固件库、LVGL、libc 或 FreeRTOS。

内存与栈限制以 ABI 和构建工具为准：Flash <=512000 B，静态 RAM <102400 B，单函数静态栈帧 <=1024 B；这些检查不证明总调用栈或动态堆安全。避免大局部数组、VLA 和未经预算的动态分配。

## 验证

从 `GlassSDK/` 运行单例构建，例如 `python3 build.py build --example game/2048`；新插件沿用现有 manifest/C 源码布局，不添加示例私有 CMakeLists。

按影响选择 `tests/` 中的 unittest：电话 UI 用 `python3 -m unittest discover -s tests -p test_system_native.py`；输入用 `test_input_native.py`；打包、栈与图像布局分别查看对应测试及所需编译器。记录跳过与环境缺失，不能把 skip 当成功覆盖。

涉及 `.gmp` 交付时读取交付技能。说明模拟器验证范围以及待真机检查的显示、输入、通信和固件兼容行为。
