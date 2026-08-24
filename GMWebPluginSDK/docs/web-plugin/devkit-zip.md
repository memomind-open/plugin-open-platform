# 压缩包 DevKit 使用说明

内部体验版压缩包结构：

```text
gm-web-plugin-devkit-0.1.0/
├── README.md
├── DEVKIT-MANIFEST.json
├── package.json
├── sdk/
│   ├── gm-plugin-web-sdk.esm.js
│   └── gm-plugin-web-sdk.d.ts
├── studio/
│   └── gm-plugin-studio.mjs
├── tools/
│   └── build-mmpkg.mjs
├── examples/
│   └── counter/
├── docs/web-plugin/
└── internal/
```

对插件开发者有意义的是：

- `sdk/`：复制到 H5 工程中，通过相对路径引入。
- `studio/`：本地模拟 App、设备画面、动作按键和生命周期。
- `tools/`：把最终 H5 构建目录打包成 `.mmpkg`。
- `examples/`：已经改为本地 SDK 引用的可运行示例。
- `docs/web-plugin/`：当前草稿开发文档。
- `internal/`：Studio 运行依赖，开发者不需要直接修改或引用。

## 环境要求

- Node.js 18 或更高版本。
- Chrome、Edge、Safari 等现代浏览器。
- Studio 与打包器本身没有第三方运行时依赖。

## 五分钟体验

解压并进入目录：

```sh
unzip gm-web-plugin-devkit-0.1.0.zip
cd gm-web-plugin-devkit-0.1.0
```

运行自带示例：

```sh
node studio/gm-plugin-studio.mjs --plugin examples/counter
```

打开 `http://127.0.0.1:4173`，然后：

1. 点击“绘制到眼镜”，确认右侧出现绿色设备画面。
2. 点击 Studio 的单击、双击或头部动作按钮，确认插件收到事件。
3. 切换设备连接与生命周期，确认插件处理状态变化。
4. 查看 Bridge Inspector 中的请求、响应和事件。

打包自带示例：

```sh
node tools/build-mmpkg.mjs \
  examples/counter \
  release/counter-0.1.0.mmpkg
```

## 接入自己的 H5 项目

复制 SDK 文件到会进入最终构建产物的位置：

```sh
mkdir -p /path/to/my-plugin/public/vendor
cp sdk/gm-plugin-web-sdk.esm.js \
  /path/to/my-plugin/public/vendor/
```

不同构建工具处理静态目录的方式不同，最终必须确认 `dist/vendor/gm-plugin-web-sdk.esm.js` 存在，并且业务代码使用包内相对 URL。不要在最终插件中引用 DevKit 所在电脑的绝对路径。

调试和打包命令：

```sh
npm run build

node /path/to/devkit/studio/gm-plugin-studio.mjs \
  --plugin /path/to/my-plugin/dist

node /path/to/devkit/tools/build-mmpkg.mjs \
  /path/to/my-plugin/dist \
  /path/to/my-plugin/release/my-plugin-1.0.0.mmpkg
```

## 当前体验版边界

- Studio 当前加载静态目录，不负责启动 Vite，也没有 HMR。
- 浏览器 SDK 是单文件 ESM；类型定义单独放在 `sdk/`。
- 当前 App 只在 Debug 环境支持未签名 `.mmpkg` 侧载。
- Studio 的字体、亮度和光学效果仍需真机最终验收。
- ZIP 版本不会与未来 npm 包并行长期维护；npm 发布后以包版本为准。
