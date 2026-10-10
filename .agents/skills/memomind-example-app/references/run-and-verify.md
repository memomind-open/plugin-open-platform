# Running, testing, and troubleshooting

Run the following commands from the repository root. Replace `<name>` with the actual directory name before giving executable commands to the developer.

In Windows PowerShell, replace `python3` with `py` and quote paths containing spaces. The generator requires Python 3.8+; Web creation/SDK refresh also requires PhoneSDK's supported Node 18+. No npm dependency installation is required for generation.

## Static Web projects

```sh
node PhoneSDK/tools/run-browser-studio.mjs --plugin examples/<name> --port 4173
node PhoneSDK/tools/build-mmpkg.mjs PhoneSDK/examples/<name> PhoneSDK/dist/<name>.mmpkg
```

The launcher's relative `--plugin` path is resolved from the PhoneSDK root; packager positional paths are resolved from the command's working directory. Do not pass `PhoneSDK/examples/<name>` to the launcher. If the port is occupied, use `--port 4174` instead of terminating unrelated processes.

Open the address printed by the launcher. Select the permissions needed by the application in the host's consent dialog and start it. Missing required approvals prevent startup; this is not a stuck Bridge. Check the phone page, virtual glasses display, error states, and reload behavior. The generated Web starter saves a counter and provides a text-sync button. Use it to verify SDK ready, storage, and display behavior before replacing the application logic.

## Vite Web projects

In the new project directory, install dependencies using its lockfile and run its actual package scripts. For a tic-tac-toe-style project:

```sh
npm ci
npm test
npm run build
```

Then return to the repository root:

```sh
node PhoneSDK/tools/run-browser-studio.mjs --plugin examples/<name>/dist
node PhoneSDK/tools/build-mmpkg.mjs PhoneSDK/examples/<name>/dist PhoneSDK/dist/<name>.mmpkg
```

The Vite public/manifest.json and static assets must reach dist, with relative resource paths. Vite's own dev server is not a Bridge host; inspecting the DOM in a normal browser does not establish that device APIs work.

## Glass projects

```sh
python3 GlassSDK/build.py build --example <name>
```

For a nested example, use its relative path (such as `game/my-game`); the GMP is at `GlassSDK/build-host/.build/<relative-path>/<directory-basename>.gmp`, alongside its review sidecars. Use `--example` for projects inside `GlassSDK/examples`: `--project` defaults to a different output root that workspace scanning does not read. Open prebuilt Desktop Studio, choose the repository root through Import workspace, refresh, and select the application. Keep new projects directly under SDK/examples for compatibility with distributed Studio versions; supported scan depth depends on the release. For deeper directories or standalone projects, use Import package; recursive build discovery does not imply unlimited Studio scan depth.

For a standalone project outside SDK/examples, build with `--project /tmp/<name>`; output goes to `/tmp/<name>/.build/<name>/<name>.gmp`. Use Import package for that GMP and place its matching source manifest beside it so Studio can read pairing metadata. GlassSDK `--build-dir` can select a temporary output root. Do not rebuild and overwrite every repository GMP merely to validate a skill.

## Selecting a paired application in Desktop Studio

For custom paired applications, build both the Web package and the dedicated GMP. Follow the [discovery and automatic matching acceptance](paired-app.md#discovery-and-automatic-matching) before handoff: first confirm the companion is in Studio's catalog, then select the phone application and verify that Studio automatically chooses a compatible provider. A manually selected pair is useful for diagnosis but does not establish that automatic matching works. Refresh may preserve an explicit glasses selection; switching the phone selection can trigger matching again.

Check the selected paths and declared protocols, clear any incompatibility warning, and run one application action across the pair before exporting a new combined ZIP/QR. For a glasses-only program, disable/clear the phone component. For a phone-only program, disable the glasses component. A source file, successful single-side build, or metadata checker does not establish discovery or paired runtime success.

## Verification scope

Use targeted tests for business algorithms, codecs, and lifecycle behavior. Run PhoneSDK `npm run verify` for public SDK changes. Root tests may not automatically discover behavior tests for a new example; inspect the package.json test glob and explicitly run the new test path when necessary.

`test_system_native.py` scans new Glass directories, but its C mock targets existing examples and flags. If a new program uses additional Host capabilities, check or extend the mock's actual behavior. Do not let the new program break the example-scanning test or simply skip the new example.

For a completed application, check startup, main interactions, persistence restoration where applicable, denied/unsupported capabilities, stop/reload, disconnection, and display restoration as relevant. For layout changes, inspect a narrow phone viewport and the virtual glasses display. List optics, recording, and IMU feel as separate physical-device checks.

## Common blockers

| Symptom | Check first |
| --- | --- |
| Bridge never becomes ready | Run inside Studio/App with a relative vendor SDK module; do not open index.html directly or use only a Vite server. |
| New application is missing after refresh | Confirm the same workspace on the Studio machine, source manifest and expected GMP path, then refresh. A project-local .build output from --project is not the workspace output. Compare displayed versions/paths and use Import package to isolate discovery from pairing; build dist for Vite. |
| MMPKG validation fails | Package the final directory, ensure entry exists, validate permission objects/scopes, keep output outside input, and exclude node_modules/source project trees. |
| API denial or CAPABILITY_UNAVAILABLE | Current capabilities, actual approvals, scope, host support, and session. Use permission-debug rather than forcing successful responses. |
| Missing protocol / unhandled custom channel | First confirm the dedicated GMP is in the catalog; if absent, diagnose discovery. If present, check the actual pair, packaged requirements, provider metadata, handlers, and freshly exported ZIP. Do not remove requirements to suppress the warning. |
| H5 updates but glasses do not | Correct GMP pairing, protocol versions/IDs, device connection, and send errors. Custom messaging requires more than Browser Studio checks. |
| Headers missing in a new Glass project | Shared call_ui/asset paths, directory depth, and standalone source-root/include-dir boundaries. |
| Native build exceeds stack/RAM limits | Large local arrays, VLAs, static caches, and pointer tables. Inspect .su files and linked segments rather than raising limits. |
| Recording works only on a computer | Host audio support and the simulated source. A computer microphone result does not verify glasses recording. |
