# GM Web Plugin DevKit 0.1.0（内部体验草稿）

本压缩包用于在 npm 包正式发布前体验 GM Web 插件开发流程，包含：

- `sdk/`：App 与 Web 插件交互的浏览器 ESM SDK 和 TypeScript 类型。
- `studio/`：GM Plugin Studio 本地模拟器启动入口。
- `tools/`：最终 `.mmpkg` 打包器。
- `examples/`：使用本地 SDK 的 Counter 示例。
- `docs/web-plugin/`：草稿版对外开发文档。

环境要求：Node.js 18 或更高版本。

## 立即运行

```sh
node studio/gm-plugin-studio.mjs --plugin examples/counter
```

打开 `http://127.0.0.1:4173`。

## 在自己的插件中使用 SDK

把 `sdk/gm-plugin-web-sdk.esm.js` 复制到插件最终产物中，例如 `vendor/`：

```js
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();
await gm.ready();
```

## 打包 App 可导入文件

```sh
node tools/build-mmpkg.mjs \
  /absolute/path/to/plugin/dist \
  /absolute/path/to/release/plugin-1.0.0.mmpkg
```

完整步骤从 [docs/web-plugin/README.md](docs/web-plugin/README.md) 开始阅读，压缩包专项说明见 [docs/web-plugin/devkit-zip.md](docs/web-plugin/devkit-zip.md)。

## 未来 npm 迁移

正式发布后计划改为：

```sh
npm install @memomind/gm-plugin-web-sdk
npm install --save-dev @memomind/gm-plugin-studio
npx gm-plugin-studio --plugin ./dist
```

迁移到 npm 不改变 Bridge API、manifest 或 `.mmpkg` 格式。
