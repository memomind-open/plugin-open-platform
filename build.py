#!/usr/bin/env python3
"""Incrementally build every plugin in the Plugin Open Platform workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass


ROOT = pathlib.Path(__file__).resolve().parent
WEB_SDK = ROOT / "WebSDK"
GLASS_SDK = ROOT / "GlassSDK"
STATE_PATH = ROOT / ".build" / "build-state.json"
ALWAYS_IGNORED = {".build", ".git", "__pycache__"}
GENERATED_DIRECTORIES = {"dist", "node_modules"}


@dataclass(frozen=True)
class WebPlugin:
    """A discovered Web plugin and the directory that must be packaged."""

    key: str
    workspace: pathlib.Path
    package_root: pathlib.Path
    manifest: dict[str, object]
    has_build_script: bool

    @property
    def output(self) -> pathlib.Path:
        version = str(self.manifest["version"])
        relative = self.workspace.relative_to(WEB_SDK / "examples")
        name = "-".join(relative.parts)
        return WEB_SDK / "dist" / f"{name}-{version}.mmpkg"


def run(command: list[object], cwd: pathlib.Path | None = None) -> None:
    rendered = [str(item) for item in command]
    print("+", " ".join(rendered), flush=True)
    subprocess.run(rendered, cwd=cwd, check=True)


def iter_files(root: pathlib.Path, ignored: set[str]):
    """Yield build inputs deterministically without generated directories."""
    if not root.exists():
        return
    for directory, names, files in os.walk(root):
        names[:] = sorted(
            name for name in names
            if name not in ignored and not name.startswith(".tmp-")
        )
        base = pathlib.Path(directory)
        for name in sorted(files):
            path = base / name
            if not path.is_symlink():
                yield path


def fingerprint(roots: list[pathlib.Path], ignored: set[str] = ALWAYS_IGNORED) -> str:
    digest = hashlib.sha256()
    for root in sorted(roots, key=lambda item: item.as_posix()):
        if root.is_file():
            files = [root]
        else:
            files = list(iter_files(root, ignored))
        for path in files:
            relative = path.relative_to(ROOT).as_posix().encode("utf-8")
            metadata = path.stat()
            digest.update(len(relative).to_bytes(4, "little"))
            digest.update(relative)
            # Match normal build-system semantics: touching an input is a
            # change even when its bytes are identical. Content hashing still
            # catches copied files whose timestamp happens to be preserved.
            digest.update(f"{metadata.st_size}:{metadata.st_mtime_ns}".encode("ascii"))
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
    return digest.hexdigest()


def read_json(path: pathlib.Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot read {path.relative_to(ROOT)}: {error}") from error
    if not isinstance(value, dict):
        raise RuntimeError(f"{path.relative_to(ROOT)} must contain a JSON object")
    return value


def nearest_web_workspace(manifest: pathlib.Path) -> tuple[pathlib.Path, dict[str, object] | None]:
    current = manifest.parent
    examples = WEB_SDK / "examples"
    while current != examples:
        package_json = current / "package.json"
        if package_json.is_file():
            return current, read_json(package_json)
        current = current.parent
    return manifest.parent, None


def discover_web_plugins() -> list[WebPlugin]:
    examples = WEB_SDK / "examples"
    plugins: list[WebPlugin] = []
    claimed_workspaces: set[pathlib.Path] = set()
    for manifest_path in sorted(examples.rglob("manifest.json")):
        if any(part in ALWAYS_IGNORED | GENERATED_DIRECTORIES for part in manifest_path.parts):
            continue
        manifest = read_json(manifest_path)
        for field in ("id", "name", "version", "entry"):
            if field not in manifest:
                raise RuntimeError(
                    f"{manifest_path.relative_to(ROOT)} is missing required field {field!r}"
                )
        workspace, package_json = nearest_web_workspace(manifest_path)
        if workspace in claimed_workspaces:
            continue
        scripts = package_json.get("scripts", {}) if package_json else {}
        has_build_script = isinstance(scripts, dict) and isinstance(scripts.get("build"), str)
        package_root = workspace / "dist" if has_build_script else manifest_path.parent
        plugins.append(WebPlugin(
            key=f"web:{workspace.relative_to(WEB_SDK).as_posix()}",
            workspace=workspace,
            package_root=package_root,
            manifest=manifest,
            has_build_script=has_build_script,
        ))
        claimed_workspaces.add(workspace)
    return plugins


def glass_inputs() -> list[pathlib.Path]:
    return [
        ROOT / "build.py",
        GLASS_SDK / "examples",
        GLASS_SDK / "include",
        GLASS_SDK / "build.py",
        GLASS_SDK / "build-host" / "tools",
    ]


def expected_glass_outputs() -> list[pathlib.Path]:
    output_root = GLASS_SDK / "build-host" / ".build"
    outputs = []
    for manifest in sorted((GLASS_SDK / "examples").rglob("manifest.json")):
        plugin_dir = manifest.parent
        if any(plugin_dir.rglob("*.c")):
            relative = plugin_dir.relative_to(GLASS_SDK / "examples")
            outputs.append(output_root / relative / f"{plugin_dir.name}.gmp")
    return outputs


def load_state() -> dict[str, object]:
    if not STATE_PATH.is_file():
        return {"version": 1, "targets": {}, "dependencies": {}}
    try:
        state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"version": 1, "targets": {}, "dependencies": {}}
    if not isinstance(state, dict) or state.get("version") != 1:
        return {"version": 1, "targets": {}, "dependencies": {}}
    if not isinstance(state.get("targets"), dict):
        state["targets"] = {}
    if not isinstance(state.get("dependencies"), dict):
        state["dependencies"] = {}
    return state


def save_state(state: dict[str, object]) -> None:
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    temporary = STATE_PATH.with_suffix(".tmp")
    temporary.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, STATE_PATH)


def npm_command() -> str:
    executable = shutil.which("npm.cmd" if os.name == "nt" else "npm")
    if executable is None:
        raise RuntimeError("Node.js 18+ and npm are required to build Web plugins")
    return executable


def dependency_fingerprint(workspace: pathlib.Path) -> str:
    inputs = [workspace / "package.json"]
    lockfile = workspace / "package-lock.json"
    if lockfile.is_file():
        inputs.append(lockfile)
    return fingerprint(inputs)


def build_web_plugin(plugin: WebPlugin, state: dict[str, object]) -> None:
    if plugin.has_build_script:
        dependencies = state["dependencies"]
        dependency_key = plugin.workspace.relative_to(ROOT).as_posix()
        current_dependencies = dependency_fingerprint(plugin.workspace)
        if (not (plugin.workspace / "node_modules").is_dir()
                or dependencies.get(dependency_key) != current_dependencies):
            install = "ci" if (plugin.workspace / "package-lock.json").is_file() else "install"
            run([npm_command(), install], cwd=plugin.workspace)
            dependencies[dependency_key] = current_dependencies
        run([npm_command(), "run", "build"], cwd=plugin.workspace)
    if not (plugin.package_root / "manifest.json").is_file():
        raise RuntimeError(
            f"Web build did not produce {plugin.package_root.relative_to(ROOT) / 'manifest.json'}"
        )
    run([
        shutil.which("node") or "node",
        WEB_SDK / "tools" / "build-mmpkg.mjs",
        plugin.package_root,
        plugin.output,
    ])


def build_once(target: str, force: bool = False, quiet: bool = False) -> bool:
    state = load_state()
    targets: dict[str, str] = state["targets"]
    built_anything = False

    if target in ("all", "glass"):
        key = "glass:all"
        current = fingerprint(glass_inputs())
        outputs = expected_glass_outputs()
        if force or targets.get(key) != current or not outputs or any(not path.is_file() for path in outputs):
            run([sys.executable, GLASS_SDK / "build.py"])
            targets[key] = current
            built_anything = True

    if target in ("all", "web"):
        packager = WEB_SDK / "tools" / "build-mmpkg.mjs"
        discovered_keys = set()
        for plugin in discover_web_plugins():
            discovered_keys.add(plugin.key)
            ignored = ALWAYS_IGNORED | GENERATED_DIRECTORIES if plugin.has_build_script else ALWAYS_IGNORED
            current = fingerprint([ROOT / "build.py", plugin.workspace, packager], ignored)
            if force or targets.get(plugin.key) != current or not plugin.output.is_file():
                print(f"WebSDK: building {plugin.workspace.relative_to(WEB_SDK)}")
                build_web_plugin(plugin, state)
                targets[plugin.key] = current
                built_anything = True
        for key in list(targets):
            if key.startswith("web:") and key not in discovered_keys:
                del targets[key]

    save_state(state)
    if not built_anything and not quiet:
        print("All selected plugins are up to date")
    return built_anything


def list_plugins() -> None:
    print("Web plugins:")
    for plugin in discover_web_plugins():
        kind = "build + package" if plugin.has_build_script else "package"
        print(f"  {plugin.workspace.relative_to(ROOT)} -> {plugin.output.relative_to(ROOT)} ({kind})")
    print("Glasses plugins:")
    for output in expected_glass_outputs():
        print(f"  {output.relative_to(ROOT)}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Discover and incrementally build WebSDK and GlassSDK plugins"
    )
    parser.add_argument("target", nargs="?", choices=("all", "web", "glass"), default="all")
    parser.add_argument("--force", action="store_true", help="rebuild every selected plugin")
    parser.add_argument("--watch", action="store_true", help="keep scanning and rebuild after changes")
    parser.add_argument("--list", action="store_true", help="list discovered plugins without building")
    args = parser.parse_args()
    if args.list:
        list_plugins()
        return 0
    if not args.watch:
        build_once(args.target, args.force)
        return 0
    print("Watching for plugin changes; press Ctrl+C to stop")
    first = True
    try:
        while True:
            build_once(args.target, args.force and first, quiet=not first)
            first = False
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopped")
        return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"build: {error}", file=sys.stderr)
        raise SystemExit(1)
