# Repository contribution rules

GMPluginSDK is a public repository for international developers. All public
repository content and Git metadata must be understandable without access to
internal company systems.

## Commit messages

- Write the commit subject and body in English.
- Use Conventional Commits:
  `<type>(<scope>): <imperative summary>`.
- Allowed types are `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`,
  `ci`, `chore`, and `revert`.
- Keep the subject concise, specific, and no longer than 72 characters.
- Use the body to explain why the change is needed, what changed, and how it
  was tested when that information is useful.
- Do not use internal-only Chinese commit templates or require Hiper,
  `STORY-*`, `BUG-*`, or `REFER-ID` fields. Add an issue reference only when a
  relevant public GitHub issue exists or the user explicitly requests one.
- Do not rewrite or force-push commits that have already been published.

Example:

```text
docs(sdk): reorganize developer documentation

Split ABI, graphics, installation, and security details into focused guides.
Add the 2048 game to the maintained example and full-build lists.

Tested with:
- ./gm-build build --example game/2048
- ./gm-build all
```

## Branches and pull requests

- Do not push changes directly to `main`.
- Use a short English branch name and open a pull request into `main`.
- Write pull request titles and descriptions in English.
