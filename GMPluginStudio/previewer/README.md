# GM Plugin Previewer

GM Plugin Previewer 是一个不依赖眼镜真机的 Windows 桌面预览工具。它直接加载 SDK 生成的 `.gmp` 文件，在软件中执行插件的 RV32 二进制代码，并模拟眼镜固件提供的 Host API、LVGL 绘图接口、GRAY4 帧缓冲和输入设备。

它的目标是完成两件事：

- 软件功能闭环：验证 `.gmp` 能否通过格式校验、重定位、入口检查和生命周期调用，并测试按钮、IMU、蓝牙、设备状态等逻辑。
- 视觉近似闭环：按照眼镜的 600×350 逻辑分辨率、4 位灰度和 LVGL 8.3 接口语义预览 UI。

预览器直接使用固件的 `lv_font_xgimi_17/20` 字库，因此字体的字形、字符步进、基线和行高来自真机资源。预览结果仍不是光学真机仿真；LVGL 主题细节、屏幕亮度、光机成像、刷新时序、IMU 噪声和蓝牙链路等效果以真机为准。

## 直接运行

已构建的 Windows x64 程序位于：

```text
build-windows/GMPluginPreviewer.exe
```

运行后可以：

1. 点击 `Open .gmp`，或把 `.gmp` 文件拖入窗口；
2. 查看 600×350 的绿色单色视觉预览；
3. 使用 `Raise / Lower / Turn left / Turn right` 每次按 3° 步进模拟头部动作；
4. 使用 `Nod / Left / Right / Shake` 发送固件已识别的 IMU 手势事件；
5. 发送单击、双击和长按按钮事件；
6. 修改电池、佩戴、充电、三轴陀螺仪和俯仰角等模拟状态；
7. 以指定 channel 向插件发送模拟手机 Bluetooth 消息；
8. 在底部查看插件日志、Host 日志以及插件发出的 Bluetooth 消息。

头部步进与手势事件是两条独立测试路径：头部步进更新 `imu_read()` 返回的原始数据；`Nod / Left / Right / Shake` 则直接测试插件的 `on_event`。抬头和低头会持久修改 Host API 提供的绝对 `pitch_degrees`。当前 Host API 没有绝对 yaw 字段，所以左右角度只显示在预览器中，插件会收到一次短暂的三轴 gyro 动作序列。

也可以把 `.gmp` 作为命令行参数：

```powershell
.\build-windows\GMPluginPreviewer.exe C:\plugins\my-plugin.gmp
```

## 构建

当前开发环境可在 Linux 上交叉构建 Windows EXE：

```bash
./build-windows.sh
```

如需使用固件字库，可以在配置 CMake 时显式指定字库目录：

```powershell
cmake -S . -B build-windows -DGM_PREVIEW_FONT_DIR=C:\path\to\fonts
```

目录中需要包含：

```text
fonts/
├── lv_font_xgimi_17.bin
└── lv_font_xgimi_20.bin
```

字库被编译进 EXE，运行时不需要额外携带字体文件。

在 Linux 上构建无界面的验证工具：

```bash
./build.sh
```

CLI 示例：

```bash
./build/gmplugin-preview-cli ../../GMPluginSDK/build-host/lvgl_ui/lvgl_ui.gmp \
  --frames 30 --pgm /tmp/lvgl-ui.pgm

./build/gmplugin-preview-cli ../../GMPluginSDK/build-host/bluetooth/bluetooth.gmp \
  --bt 1 "Hello plugin"
```

## 已实现能力

### `.gmp` 加载与执行

- GMP v1 magic、版本、大小、ABI 和 CRC32 校验；
- 与当前固件相同的 image copy、BSS 清零和 base relocation；
- 插件入口、descriptor 和回调地址范围校验；
- `on_load`、`on_start`、`on_resume`、`on_loop`、`on_event`、`on_suspend`、`on_stop`、`on_unload` 生命周期；
- RV32I、M、A、F、C 指令解释执行，带 guest 内存边界和单次回调指令预算；
- 插件二进制不会作为 x86 本机代码直接执行。

### Host API 模拟

- 日志、单调时钟、Host 堆分配和泄漏报告；
- 600×350、30 Hz、GRAY4 显示信息；
- 两个 175 行同步区域的直接帧缓冲 lock/unlock；
- 显示开关、亮度、光学距离、高度和自动亮度阻止；
- Bluetooth 收发模拟；
- IMU enable/read、可配置原始数据、3° 头部姿态步进和手势事件；
- 电量、充电、佩戴、locale、connection 和 app exit；
- demo extension 和 LZ4 extension。

### LVGL 兼容层

实现了公开 `gm_plugin_lvgl_api_t` 的完整 1.0 函数表，包括：

- root、通用 object、label、arc 和 line；
- position、size、align、flags 和常用 style；
- 按固件 XBF 字库进行文本测量、换行和 2 bpp 字形绘制；
- 默认字体使用 34 像素行高、7 像素基线；大字体使用 41 像素行高、9 像素基线；
- 常用背景、边框、文字、线条和圆弧样式的近似绘制。

这里模拟的是 SDK 公开的 LVGL API 语义，并没有把眼镜固件中的 LVGL 二进制整体搬进 PC。因此 UI 布局和交互适合开发期预览，像素级截图不能作为真机验收标准。

## 回归验证

运行：

```bash
./tests/smoke.sh
```

回归脚本会运行 `GMPluginSDK/build-host` 中的所有维护示例，并额外验证：

- Bluetooth message 进入插件并产生回包；
- button 和 IMU gesture 事件进入插件；
- 方向手势会同时生成一次已识别事件和一段回正的 Raw IMU 轨迹，兼容对固件方向
  事件去重、改用 Raw IMU 识别方向的设备插件；
- framebuffer 示例产生非空的 PGM 画面。

本次实现已通过以下 15 个示例：

```text
minimal       input          imu             device_state
bluetooth     extension      lz4             framebuffer
lvgl_ui       scene_bridge   2048            breakout
jet_runner    snake          tetris
```

## 代码结构

```text
previewer/
├── src/rv32.*           RV32IMAFC guest 执行器与隔离内存
├── src/previewer.*      GMP 加载器、Host API、LVGL 场景和渲染核心
├── src/main_win32.cpp   Windows 桌面界面
├── src/main_cli.cpp     无界面测试入口
├── tests/smoke.sh       SDK 示例回归
├── build-windows.sh     Windows x64 交叉构建
└── build.sh             本机 CLI 构建
```

## 当前边界

- 字形位图和字体度量来自固件，但 LVGL 主题、对象绘制、混色和裁剪仍为兼容实现，不保证整幅画面逐像素一致；
- F 扩展的舍入和浮点异常标志使用宿主机近似语义；SDK 当前维护示例主要使用整数路径；
- 模拟 Bluetooth 只记录插件出站包，不连接真实手机；
- IMU 是确定性的可配置样本，不模拟传感器噪声和采样延迟；
- 不模拟 SPI、光机、固件任务调度、内存压力和真实性能；
- `.gmp` 仍属于不可信输入。虽然 guest 指令和内存不直接映射为本机执行，正式对外分发前仍应补充 parser fuzz、资源上限和安全审计。
