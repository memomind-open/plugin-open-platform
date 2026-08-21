# 兼容性与真机边界

Studio 的 Bridge 方法、事件和 GRAY_4 位图目标是契约一致。首版 Canvas 文本不是固件 LVGL 和专有字体的像素级替代，亮度、光学距离、镜片畸变和实际手势阈值仍需真机验收。

每个 Studio 运行实例都应显示 Bridge 版本和 device profile。未来 profile 将由固件侧版本化生成，避免手工复制常量产生漂移。
