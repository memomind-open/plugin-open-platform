---
name: memomind-example-app
description: Build runnable MemoMind plugins or paired applications from developer requirements and existing examples, including example selection, project creation, implementation, pairing, preview, and packaging. Use for timers, games, readers, dashboards, pets, and similar example-based applications.
---

# From a requirement to a runnable application

Use this skill for the complete application workflow. Read the root `AGENTS.md`, choose an approach using [example selection and development recipes](references/example-recipes.md), then load the relevant [Glass skill](../memomind-glass-plugin/SKILL.md) or [Web skill](../memomind-web-plugin/SKILL.md). Ordinary H5 plugins do not require learning raw GM Bluetooth framing first.

Developers can adapt the timer, game, controller, and reader requests in [sample task prompts](references/developer-prompts.md).

## Starting point and defaults

Translate the request into where the application runs (phone, glasses, or both), its main interactions, data sources, persistence requirements, and observable acceptance criteria. When unspecified, follow the architecture of the closest example, explain the choice, and implement it. Ask only when a missing decision changes the core experience or an external interface is required but unavailable.

- Independent glasses execution, local games, or efficient native rendering: a Glass plugin.
- Phone-owned state with text/Canvas rendering and input on the glasses: a Web plugin with the existing `web_bridge`; a new GMP is usually unnecessary.
- A dedicated state or streaming protocol between the two platforms: a Web plugin paired with a dedicated Glass plugin; define the messages before implementation.
- Phone-only functionality: a Web plugin with unnecessary device requirements and permissions removed; do not introduce a glasses dependency.

For a new application, create a new directory and independent identity while preserving the reference example. For a request to modify an existing example, work in that directory without forcing a copy. To generate a minimal project, run this tool from the repository root:

```sh
python3 .agents/skills/memomind-example-app/scripts/create_example.py \
  --kind web --name focus-timer --id com.example.focus-timer --title "Focus Timer"
# For a native LVGL starter, use glass instead of web and choose its own name and ID.
```

The tool refuses to overwrite an existing directory. Web projects bundle the current local SDK; Glass projects use the current lvgl_ui example and copy its shared call-UI header. It creates a runnable starting point, **not the complete requested application**. For complex assets or protocols, read and adapt the relevant example rather than using a large application as an empty template.

## Completing the implementation

1. Get the minimal page or native UI running once, then add state, interactions, and real data. Use current public APIs without reimplementing the Bridge handshake. A result displayed only in the phone DOM does not establish that glasses rendering works.
2. Update the new identity in the manifest, page title, storage keys, and relevant package metadata. When copying an example, check relative includes/imports, the vendored SDK, asset paths, and device pairing IDs. Avoid global string replacements or claiming protocol capabilities that are not implemented.
3. For paired applications, check both implementations using [paired protocols and integration](references/paired-app.md). Standard display/device APIs reuse the existing Web Bridge protocols.
4. Adapt the required features and remove unused permissions, remote domains, audio/image assets, and UI from the new project. Preserve the source example. Show real network and storage failures, and make cancellation, permission denial, and disconnection recoverable. Do not present demonstration data as real results.
5. Verify business logic with targeted checks, then preview and package the application. A request to build a program calls for code and a runnable package, not just a design or prompt. Avoid incidental changes to the SDK ABI, private Studio, or unrelated examples.

Generator regression checks: run `python3 .agents/skills/memomind-example-app/scripts/test_create_example.py` from the repository root. They cover real Web packaging, overwrite refusal, per-project SDK refresh, and Glass header compilation; application behavior still needs its own acceptance checks.

## Acceptance and handoff

Follow [running, testing, and troubleshooting](references/run-and-verify.md) for commands in the correct directory. Distinguish source directories from built dist output and Browser Studio from Desktop Studio. Confirm that the manifest, modules, and assets are present in the final package.

Give the developer the new directory, startup commands, the GMP to select (if applicable), package paths, completed checks, and outstanding device checks. Briefly identify where to modify application state, rendering, and protocol handling so the developer can run and extend the application without rereading the entire SDK manual.
