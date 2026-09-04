#!/usr/bin/env python3
"""Incrementally build every plugin in the Plugin Open Platform workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import time


ROOT = pathlib.Path(__file__).resolve().parent
PHONE_SDK = ROOT / "PhoneSDK"
GLASS_SDK = ROOT / "GlassSDK"
STATE_PATH = ROOT / ".build" / "build-state.json"
ALWAYS_IGNORED = {".build", ".git", "__pycache__"}


def run(
    command: list[object],
    cwd: pathlib.Path | None = None,
    announce: bool = True,
) -> None:
    rendered = [str(item) for item in command]
    if announce:
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


def fingerprint(roots: list[pathlib.Path]) -> str:
    digest = hashlib.sha256()
    for root in sorted(roots, key=lambda item: item.as_posix()):
        files = [root] if root.is_file() else list(iter_files(root, ALWAYS_IGNORED))
        for path in files:
            relative = path.relative_to(ROOT).as_posix().encode("utf-8")
            metadata = path.stat()
            digest.update(len(relative).to_bytes(4, "little"))
            digest.update(relative)
            digest.update(f"{metadata.st_size}:{metadata.st_mtime_ns}".encode("ascii"))
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
    return digest.hexdigest()


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
        return {"version": 1, "targets": {}}
    try:
        state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"version": 1, "targets": {}}
    if not isinstance(state, dict) or state.get("version") != 1:
        return {"version": 1, "targets": {}}
    if not isinstance(state.get("targets"), dict):
        state["targets"] = {}
    return state


def save_state(state: dict[str, object]) -> None:
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    temporary = STATE_PATH.with_suffix(".tmp")
    temporary.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, STATE_PATH)


def build_glass_once(force: bool = False) -> bool:
    state = load_state()
    targets: dict[str, str] = state["targets"]
    key = "glass:all"
    current = fingerprint(glass_inputs())
    outputs = expected_glass_outputs()
    if not force and targets.get(key) == current and outputs and all(
        path.is_file() for path in outputs
    ):
        return False
    run([sys.executable, GLASS_SDK / "build.py"])
    targets[key] = current
    save_state(state)
    return True


def build_phone_once(force: bool = False, quiet: bool = False) -> None:
    command: list[object] = [sys.executable, PHONE_SDK / "build.py"]
    if force:
        command.append("--force")
    if quiet:
        command.append("--quiet")
    run(command, announce=not quiet)


def build_once(target: str, force: bool = False, quiet: bool = False) -> bool:
    built_glass = target in ("all", "glass") and build_glass_once(force)
    delegated_phone = target in ("all", "web")
    if delegated_phone:
        build_phone_once(force, quiet)
    if not built_glass and not delegated_phone and not quiet:
        print("All selected plugins are up to date")
    return built_glass or delegated_phone


def list_plugins() -> None:
    run([sys.executable, PHONE_SDK / "build.py", "--list"])
    print("Glasses plugins:")
    for output in expected_glass_outputs():
        print(f"  {output.relative_to(ROOT)}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Incrementally build PhoneSDK Web plugins and GlassSDK plugins.",
        epilog="""Build examples:
  Ubuntu/macOS:
    ./build.py              Build all changed PhoneSDK and GlassSDK plugins
    ./build.py web          Build changed PhoneSDK Web plugins only
    ./build.py glass        Build changed GlassSDK plugins only

  Windows PowerShell:
    py build.py             Build all changed PhoneSDK and GlassSDK plugins
    py build.py web         Build changed PhoneSDK Web plugins only
    py build.py glass       Build changed GlassSDK plugins only

  Common options:
    --force                 Rebuild every selected plugin
    --watch                 Watch for changes and rebuild continuously
    --list                  Show discovered inputs and output files

Outputs:
  PhoneSDK/dist/*.mmpkg
  GlassSDK/build-host/.build/**/*.gmp""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        add_help=False,
    )
    parser.add_argument(
        "-h", "--h", "-help", "--help",
        action="help",
        help="show this build guide and exit",
    )
    parser.add_argument("target", nargs="?", choices=("all", "web", "glass"), default="all")
    parser.add_argument("--force", action="store_true", help="rebuild every selected plugin")
    parser.add_argument("--watch", action="store_true", help="keep scanning and rebuild changes")
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
