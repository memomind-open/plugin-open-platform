# Web plugin examples

`examples/` is the single home for runnable Web plugin examples and complete
plugin workspaces maintained with this SDK. Create one self-contained
directory at any depth for each plugin:

```text
examples/
|-- life-desk/
|   |-- index.html
|   |-- manifest.json
|   `-- ...
`-- vendor/
    `-- <another-plugin>/
        |-- index.html
        |-- manifest.json
        `-- ...
```

PhoneSDK Browser Studio currently runs one plugin at a time:

```sh
node tools/run-browser-studio.mjs --plugin examples/<relative-path>
```

Use a different port when running more than one Browser Studio instance:

```sh
node tools/run-browser-studio.mjs --plugin examples/<relative-path> --port 4174
```

Build a distributable package outside the plugin source directory:

```sh
npm run pack:plugin -- examples/<relative-path> dist/<plugin-name>.mmpkg
```

## Included plugins

| Directory | Plugin ID | Development form |
| --- | --- | --- |
| `app-counter/` | `com.memomind.demo.counter` | App built-in counter example |
| `audio-capture-lab/` | `com.memomind.demo.audio-capture-lab` | Native recording and binary-stream parameter lab |
| `talking-pet/` | `com.memomind.example.talking-pet` | Interactive talking-pet game |
| `novel-reader/` | `com.memomind.example.novel-reader` | TXT/EPUB reader with glasses-side text layout and standalone illustration pages |
| `tic-tac-toe/` | `com.memomind.demo.tictactoe` | TypeScript/Vite source project |
| `weather/` | `com.memomind.demo.weather` | Static HTML/CSS/JavaScript |
| `fighter-controller/` | `com.memomind.fighter.controller` | Multi-touch GMP game controller |
| `life-desk/` | `com.memomind.lifedesk` | Scene display and device-event demo |

Static plugins can run directly in Desktop Studio or PhoneSDK Browser Studio.
Build TypeScript/Vite plugins first so either tool can select their `dist/`
output; generated `dist/` directories remain ignored by Git. Desktop Studio
recursively discovers every runnable `manifest.json` below `examples/`, so a
new plugin does not require a Studio source change or a predefined directory
name.
