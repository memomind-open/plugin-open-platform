# Web plugin examples

`examples/` is the single home for all runnable Web plugin examples and
complete plugin workspaces maintained with this SDK. Create one self-contained
subdirectory for each plugin:

```text
examples/
├── life-desk/
│   ├── index.html
│   ├── manifest.json
│   └── ...
└── <another-plugin>/
    ├── index.html
    ├── manifest.json
    └── ...
```

The Studio currently runs one plugin at a time:

```sh
node tools/run-browser-studio.mjs --plugin examples/<plugin-name>
```

Use a different port when running more than one Studio instance:

```sh
node tools/run-browser-studio.mjs --plugin examples/<plugin-name> --port 4174
```

Build a distributable package outside the plugin source directory:

```sh
npm run pack:plugin -- examples/<plugin-name> dist/<plugin-name>.mmpkg
```

## Included plugins

The following source workspaces are included:

| Directory | Plugin ID | Development form |
| --- | --- | --- |
| `basic-counter/` | `com.memomind.example.counter` | Minimal static SDK example |
| `app-counter/` | `com.memomind.demo.counter` | App built-in counter example |
| `talking-pet/` | `com.memomind.example.talking-pet` | Interactive talking-pet game |
| `tic-tac-toe/` | `com.memomind.demo.tictactoe` | TypeScript/Vite source project |
| `weather/` | `com.memomind.demo.weather` | Static HTML/CSS/JavaScript |
| `fighter-controller/` | `com.memomind.fighter.controller` | Multi-touch GMP game controller |
| `life-desk/` | `com.memomind.lifedesk` | Scene display and device-event demo |

Static plugins can run directly in GM Plugin Studio. Build TypeScript/Vite
plugins first so Studio can select their `dist/` output; generated `dist/`
directories remain ignored by Git.
