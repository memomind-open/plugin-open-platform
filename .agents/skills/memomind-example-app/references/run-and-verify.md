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

The GMP is at `GlassSDK/build-host/.build/<name>/<name>.gmp`, alongside its review sidecars. Open prebuilt Desktop Studio, choose the repository root through Import workspace, refresh, and select the application. Studio scans plugins one to three levels below collection directories. For deeper directories or standalone projects, use Import package; recursive build discovery does not imply unlimited Studio scan depth.

When generating with `--output /tmp/...`, build with `--project /tmp/...`; output goes into the project's own `.build/`. GlassSDK `--build-dir` can select a temporary output root. Do not rebuild and overwrite every repository GMP merely to validate a skill.

## Verification scope

Use targeted tests for business algorithms, codecs, and lifecycle behavior. Run PhoneSDK `npm run verify` for public SDK changes. Root tests may not automatically discover behavior tests for a new example; inspect the package.json test glob and explicitly run the new test path when necessary.

`test_system_native.py` scans new Glass directories, but its C mock targets existing examples and flags. If a new program uses additional Host capabilities, check or extend the mock's actual behavior. Do not let the new program break the example-scanning test or simply skip the new example.

For a completed application, check startup, main interactions, persistence restoration where applicable, denied/unsupported capabilities, stop/reload, disconnection, and display restoration as relevant. For layout changes, inspect a narrow phone viewport and the virtual glasses display. List optics, recording, and IMU feel as separate physical-device checks.

## Common blockers

| Symptom | Check first |
| --- | --- |
| Bridge never becomes ready | Run inside Studio/App with a relative vendor SDK module; do not open index.html directly or use only a Vite server. |
| New application is missing after refresh | Correct manifest location, supported Studio discovery depth, successful build, and matching output path; build dist for Vite. |
| MMPKG validation fails | Package the final directory, ensure entry exists, validate permission objects/scopes, keep output outside input, and exclude node_modules/source project trees. |
| API denial or CAPABILITY_UNAVAILABLE | Current capabilities, actual approvals, scope, host support, and session. Use permission-debug rather than forcing successful responses. |
| H5 updates but glasses do not | Correct GMP pairing, protocol versions/IDs, device connection, and send errors. Custom messaging requires more than Browser Studio checks. |
| Headers missing in a new Glass project | Shared call_ui/asset paths, directory depth, and standalone source-root/include-dir boundaries. |
| Native build exceeds stack/RAM limits | Large local arrays, VLAs, static caches, and pointer tables. Inspect .su files and linked segments rather than raising limits. |
| Recording works only on a computer | Host audio support and the simulated source. A computer microphone result does not verify glasses recording. |
