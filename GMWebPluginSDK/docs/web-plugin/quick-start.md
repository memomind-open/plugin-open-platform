# 快速开始

当前内部体验版通过 `gm-web-plugin-devkit-<version>.zip` 分发。解压后包含浏览器 SDK、Studio、打包器、示例和本开发文档，不需要连接 npm 仓库。

## 1. 引入本地 SDK

把 DevKit 中的 SDK 文件复制进插件工程：

```sh
mkdir -p ./vendor
cp /path/to/gm-web-plugin-devkit/sdk/gm-plugin-web-sdk.esm.js ./vendor/
```

插件代码通过相对路径引入：

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();
await gm.ready();

await gm.display.updateText({
  id: 1,
  x: 20,
  y: 20,
  width: 300,
  height: 60,
  border: 1,
  radius: 8,
  text: 'Hello GM',
});
```

SDK 会自动识别真实 App WebView 或 Studio 模拟宿主，插件业务代码不需要维护两套实现。

## 2. 使用 Studio 调试

如果项目是纯静态 H5，插件目录根部有 `index.html`：

```sh
node /path/to/gm-web-plugin-devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin
```

如果项目使用 Vite、Webpack 等构建工具，先构建，再加载最终产物目录：

```sh
npm run build

node /path/to/gm-web-plugin-devkit/studio/gm-plugin-studio.mjs \
  --plugin /absolute/path/to/my-plugin/dist
```

默认打开地址是 `http://127.0.0.1:4173`。可追加 `--port 4174` 修改端口。

## 3. 生成 App 插件包

确保构建目录根部有 `manifest.json`，然后生成 `.mmpkg`：

```sh
node /path/to/gm-web-plugin-devkit/tools/build-mmpkg.mjs \
  /absolute/path/to/my-plugin/dist \
  /absolute/path/to/release/my-plugin-1.0.0.mmpkg
```

完整格式和安全限制见[最终插件包 `.mmpkg`](package-format.md)。

## 未来 npm 使用方式

ZIP 是内部体验阶段的临时分发方式。正式发布 npm 包后，SDK import 将改为：

```sh
npm install @memomind/gm-plugin-web-sdk
npm install --save-dev @memomind/gm-plugin-studio
```

```js
import { createGMPlugin } from '@memomind/gm-plugin-web-sdk';
```

Studio 将通过包内命令启动：

```sh
npx gm-plugin-studio --plugin ./dist
```

从 ZIP 迁移到 npm 不改变 Bridge API、manifest 或 `.mmpkg` 格式。
