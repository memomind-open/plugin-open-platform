# Plugin Open Platform AI Development Guide

Start with `git status --short --branch`, the root README, and the SDK documents relevant to the task. Communicate in the user's preferred language; keep shared skill documentation and starter UI text in English unless the task specifies another language. Preserve existing changes and follow the user's authorization for commits, pushes, and releases. This repository distributes the public SDKs; the website frontend and Desktop Studio source are maintained separately.

## Skill selection

Project skills live under `.agents/skills/`. Read the relevant `SKILL.md` for the task without loading every document. Tools with project-skill discovery can select them automatically or through `$skill-name`; other AI tools can read the files linked below directly.

| Task | Skill |
| --- | --- |
| Build a complete application from a requirement, or adapt and combine examples | [memomind-example-app](.agents/skills/memomind-example-app/SKILL.md) |
| Create or modify glasses C plugins, the Host ABI, drawing, input, or lifecycle behavior | [memomind-glass-plugin](.agents/skills/memomind-glass-plugin/SKILL.md) |
| Create or modify H5 plugins, Bridge calls, permissions, Browser Studio, or the Web SDK | [memomind-web-plugin](.agents/skills/memomind-web-plugin/SKILL.md) |
| Integrate an independent Bluetooth client, HUD, recording, HOGP, or business messages | [memomind-business-protocol](.agents/skills/memomind-business-protocol/SKILL.md) |
| Build, package, review, or distribute plugins and SDK artifacts | [memomind-plugin-delivery](.agents/skills/memomind-plugin-delivery/SKILL.md) |

For a new application, start with `memomind-example-app` to choose an architecture and create the project, then load the skills for the platforms involved. For an existing application fix, select the relevant platform skill directly. Combine skills for cross-platform tasks: a phone-controlled glasses game needs Web and Glass guidance, custom messages may need protocol guidance, and package delivery needs the delivery skill.

## Repository boundaries and sources of truth

- `GlassSDK/include/` defines the native ABI; `GlassSDK/docs/` explains its constraints, and `GlassSDK/examples/` provides reference implementations.
- `PhoneSDK/packages/bridge-contract`, `web-sdk`, `device-renderer`, and `studio-runtime` own contracts, public APIs, rendering, and host simulation respectively. Keep application logic in examples rather than adding it to shared packages.
- `Studio/` contains prebuilt releases. Public Browser Studio source is in `PhoneSDK/tools/browser-studio`. Fixing Desktop Studio itself requires its source; an SDK change alone does not establish that the private host has been fixed.
- Check documentation against current headers, contract implementations, and tests. When older descriptions conflict, explain the difference and update the documents relevant to the task instead of copying outdated examples or another repository's rules.
- The root build uses Python. PhoneSDK uses the npm workflow defined by its `package.json` and lockfile. This repository does not provide the website repository's pnpm quality/harness commands or its routing, account, and branch-role gates.
- SDK, example, and Studio directory relationships affect discovery and pairing. Preserve the layout documented in the root README.

For application tasks, explicitly choose phone-only, glasses-only, Web with existing Web Bridge, or a custom pair. Custom paired delivery requires both implementations and built packages, matching protocol metadata, Studio discovery and automatic matching checks, and concrete package paths. A metadata pass does not establish that Studio can discover or automatically select the companion. An explicit single-platform request stays single-platform; disable the unused Studio component. See the application skill for pairing preflight and the distinction between metadata compatibility and runtime verification.

## Implementation and acceptance

Before editing, briefly state the goal, concrete scope, observable acceptance criteria, and behavior to preserve. A message in the conversation is sufficient. Select targeted tests or builds appropriate to the change; do not add tests merely for small documentation or styling edits.

For functional fixes, verify the failure cause and corrected behavior. Cover relevant permission denial, unsupported capabilities, disconnection, and stop/restart states. Do not remove assertions, skip failures, or fabricate successful results to complete a task.

Finish by checking `git diff --check` and the actual scope of changes. Builds may update tracked `.gmp`, `.mmpkg`, review attachments, or other artifacts. Include only artifacts required for the delivery, avoiding incidental updates to all examples or Studio binaries. Do not overwrite or revert artifacts belonging to existing user work.

Report changes, checks and their results, generated package locations, and unverified items. Automated tests, simulators, physical devices, and deployment provide different evidence; only report steps actually performed as complete.
