# Web plugins

`plugins/` contains the complete Web plugin workspaces maintained with this
SDK. Create one self-contained subdirectory for each plugin:
Create one self-contained subdirectory for each plugin:

```text
plugins/
├── gm-life-desk/
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
node tools/studio-cli.mjs --plugin plugins/<plugin-name>
```

Use a different port when running more than one Studio instance:

```sh
node tools/studio-cli.mjs --plugin plugins/<plugin-name> --port 4174
```

Build a distributable package outside the plugin source directory:

```sh
npm run pack:plugin -- plugins/<plugin-name> dist/<plugin-name>.mmpkg
```

## Included plugins

The following source workspaces are included:

| Directory | Plugin ID | Development form |
| --- | --- | --- |
| `counter/` | `com.memomind.demo.counter` | Static HTML/CSS/JavaScript |
| `tictactoe/` | `com.memomind.demo.tictactoe` | TypeScript/Vite source project |
| `weather/` | `com.memomind.demo.weather` | Static HTML/CSS/JavaScript |
| `fighter-controller/` | `com.memomind.fighter.controller` | Multi-touch GMP game controller |
| `gm-life-desk/` | `com.memomind.lifedesk` | Scene display and device-event demo |

Static plugins can run directly in GM Plugin Studio. Build TypeScript/Vite
plugins first so Studio can select their `dist/` output; generated `dist/`
directories remain ignored by Git.
