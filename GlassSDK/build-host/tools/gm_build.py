#!/usr/bin/env python3
"""Cross-platform, dependency-free build driver for GMPluginSDK."""

from __future__ import annotations

import argparse
import hashlib
import importlib
import os
import pathlib
import platform
import re
import shutil
import ssl
import subprocess
import sys
import sysconfig
import tarfile
import urllib.request
import zipfile

try:
    from .gmp_serve import choose_lan_address, qr_terminal_text, serve_forever, start_server
except ImportError:
    from gmp_serve import choose_lan_address, qr_terminal_text, serve_forever, start_server


SDK = pathlib.Path(__file__).resolve().parents[2]
BUILD_INTERNALS = SDK / "build-host" / ".build"
BUILD_DEFINITION = SDK / "build-host" / "tools" / "build"
VERSION = "15.2.0-1"
RELEASE = (
    "https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/"
    f"releases/download/v{VERSION}"
)
PACKAGES = {
    ("windows", "x64"): (
        f"xpack-riscv-none-elf-gcc-{VERSION}-win32-x64.zip",
        "85ef714dacd273b1dadf4af4892774520ac01915bfa6da816a56e7e41591e09e",
    ),
    ("darwin", "x64"): (
        f"xpack-riscv-none-elf-gcc-{VERSION}-darwin-x64.tar.gz",
        "98e83f097b10163869dabffd58389ac8e4eb41bae0f67124569158655be593ea",
    ),
    ("darwin", "arm64"): (
        f"xpack-riscv-none-elf-gcc-{VERSION}-darwin-arm64.tar.gz",
        "6588e8351455fad8aca37551f0e5a5543f3346bfa9a837cf03cbd3bdd4989f8f",
    ),
    ("linux", "x64"): (
        f"xpack-riscv-none-elf-gcc-{VERSION}-linux-x64.tar.gz",
        "aaaa8060c914851a3e5ee1ba82cc3d6f80972f90638a05c6e823a37557a33758",
    ),
    ("linux", "arm64"): (
        f"xpack-riscv-none-elf-gcc-{VERSION}-linux-arm64.tar.gz",
        "4e60e2a54c16385e4e2476d08240f857495d5a61609d97e1ee49f72875a6ec1e",
    ),
}


def toolchain_cache(system: str) -> pathlib.Path:
    """Return a per-user cache outside the distributable SDK directory."""
    if system == "windows":
        local = os.environ.get("LOCALAPPDATA")
        base = pathlib.Path(local) if local else pathlib.Path.home() / ".cache"
    elif system == "darwin":
        base = pathlib.Path.home() / "Library" / "Caches"
    else:
        xdg_cache = os.environ.get("XDG_CACHE_HOME")
        base = (
            pathlib.Path(xdg_cache).expanduser()
            if xdg_cache else pathlib.Path.home() / ".cache"
        )
    return base / "GMPluginSDK" / "toolchains"


def host() -> tuple[str, str]:
    system = platform.system().lower()
    machine = platform.machine().lower()
    arch = "arm64" if machine in ("arm64", "aarch64") else "x64" if machine in ("amd64", "x86_64") else machine
    key = (system, arch)
    if key not in PACKAGES:
        raise RuntimeError(f"unsupported host: {system}/{machine}")
    return key


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, target: pathlib.Path, insecure: bool) -> None:
    context = ssl._create_unverified_context() if insecure else None
    request = urllib.request.Request(url, headers={"User-Agent": "GMPluginSDK/1"})
    partial = target.with_suffix(target.suffix + ".part")
    try:
        with urllib.request.urlopen(request, context=context) as response, partial.open("wb") as output:
            total = int(response.headers.get("Content-Length", 0))
            done = 0
            while True:
                block = response.read(1024 * 1024)
                if not block:
                    break
                output.write(block)
                done += len(block)
                if total:
                    print(f"\rDownloading {target.name}: {done * 100 // total:3d}%", end="", flush=True)
        print()
        partial.replace(target)
    except Exception:
        partial.unlink(missing_ok=True)
        raise


def discard_download(archive: pathlib.Path) -> None:
    """Best-effort removal of archives that are no longer needed."""
    for path in (archive, archive.with_suffix(archive.suffix + ".part")):
        try:
            path.unlink(missing_ok=True)
        except OSError:
            pass


def ensure_toolchain(insecure: bool = False) -> pathlib.Path:
    override = os.environ.get("GM_RISCV_TOOLCHAIN")
    if override:
        bin_dir = pathlib.Path(override).expanduser().resolve()
    else:
        system_arch = host()
        archive_name, expected = PACKAGES[system_arch]
        # Keep the large compiler outside the SDK so copying or publishing the
        # SDK never includes a host-specific toolchain. This also avoids long
        # paths and UNC extraction problems on Windows.
        cache = toolchain_cache(system_arch[0])
        root = cache / f"xpack-riscv-none-elf-gcc-{VERSION}"
        bin_dir = root / "bin"
        archive = cache / archive_name
        exe = ".exe" if system_arch[0] == "windows" else ""
        if (bin_dir / f"riscv-none-elf-gcc{exe}").is_file():
            discard_download(archive)
            return bin_dir
        cache.mkdir(parents=True, exist_ok=True)
        if not archive.is_file() or sha256(archive) != expected:
            archive.unlink(missing_ok=True)
            print(f"Fetching xPack RISC-V GCC {VERSION} for {system_arch[0]}/{system_arch[1]}")
            download(f"{RELEASE}/{archive_name}", archive, insecure)
        actual = sha256(archive)
        if actual != expected:
            archive.unlink(missing_ok=True)
            raise RuntimeError(f"SHA-256 mismatch for {archive_name}: {actual}")
        staging = cache / f".extract-{VERSION}"
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir()
        if archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as bundle:
                bundle.extractall(staging)
        else:
            with tarfile.open(archive, "r:gz") as bundle:
                # Python 3.12 added the safe extraction filter.  The SDK still
                # supports Python 3.8; its fallback is acceptable here because
                # the archive filename and SHA-256 are pinned above.
                if hasattr(tarfile, "data_filter"):
                    bundle.extractall(staging, filter="data")
                else:
                    bundle.extractall(staging)
        extracted = staging / f"xpack-riscv-none-elf-gcc-{VERSION}"
        if root.exists():
            shutil.rmtree(root)
        extracted.replace(root)
        shutil.rmtree(staging)
        discard_download(archive)
    exe = ".exe" if platform.system() == "Windows" else ""
    if not (bin_dir / f"riscv-none-elf-gcc{exe}").is_file():
        raise RuntimeError(f"RISC-V GCC not found in {bin_dir}")
    return bin_dir


def run(command: list[object]) -> None:
    rendered = [str(value) for value in command]
    print("+", " ".join(rendered))
    subprocess.run(rendered, check=True)


def tool(bin_dir: pathlib.Path, name: str) -> pathlib.Path:
    suffix = ".exe" if platform.system() == "Windows" else ""
    return bin_dir / f"riscv-none-elf-{name}{suffix}"


def resolve_build_tool(name: str, module_name: str, attribute: str) -> pathlib.Path | None:
    suffix = ".exe" if platform.system() == "Windows" else ""
    candidates = []
    try:
        module = importlib.import_module(module_name)
        candidates.append(pathlib.Path(getattr(module, attribute)) / f"{name}{suffix}")
    except (ImportError, AttributeError, TypeError):
        pass
    candidates.extend((
        shutil.which(name),
        pathlib.Path(sysconfig.get_path("scripts")) / f"{name}{suffix}",
    ))
    for candidate in candidates:
        if candidate and pathlib.Path(candidate).is_file():
            return pathlib.Path(candidate)
    return None


def tool_is_current(tool_path: pathlib.Path | None, minimum: tuple[int, int]) -> bool:
    if tool_path is None:
        return False
    result = subprocess.run(
        [str(tool_path), "--version"], capture_output=True, text=True, check=False
    )
    match = re.search(r"(\d+)\.(\d+)", result.stdout)
    return result.returncode == 0 and match is not None and (
        int(match.group(1)), int(match.group(2))
    ) >= minimum


def ensure_build_tools() -> tuple[pathlib.Path, pathlib.Path]:
    cmake = resolve_build_tool("cmake", "cmake", "CMAKE_BIN_DIR")
    ninja = resolve_build_tool("ninja", "ninja", "BIN_DIR")
    if not tool_is_current(cmake, (3, 16)) or not tool_is_current(ninja, (1, 10)):
        print("Installing CMake and Ninja build dependencies...")
        install_python_packages("cmake==3.31.6", "ninja==1.11.1.4")
        cmake = resolve_build_tool("cmake", "cmake", "CMAKE_BIN_DIR")
        ninja = resolve_build_tool("ninja", "ninja", "BIN_DIR")
    if not tool_is_current(cmake, (3, 16)) or not tool_is_current(ninja, (1, 10)):
        raise RuntimeError("CMake and Ninja could not be installed automatically")
    return cmake, ninja


def install_python_packages(*packages: str) -> None:
    command = [
        sys.executable, "-m", "pip", "install", "--disable-pip-version-check", *packages,
    ]
    try:
        run(command)
    except subprocess.CalledProcessError:
        run([sys.executable, "-m", "ensurepip", "--upgrade"])
        run(command)


def example_names() -> tuple[str, ...]:
    examples = []
    root = SDK / "examples"
    for manifest in root.rglob("manifest.json"):
        if any(manifest.parent.rglob("*.c")):
            examples.append(manifest.parent.relative_to(root).as_posix())
    return tuple(sorted(examples))


def cmake_target(example: str) -> str:
    return "gm_plugin_" + example.replace("\\", "/").strip("/").replace("/", "_")


def package_path(build_root: pathlib.Path, example: str) -> pathlib.Path:
    normalized = pathlib.PurePosixPath(example.replace("\\", "/").strip("/"))
    return build_root.joinpath(*normalized.parts, f"{normalized.name}.gmp")


def intermediate_elf_path(example: str) -> pathlib.Path:
    normalized = pathlib.PurePosixPath(example.replace("\\", "/").strip("/"))
    return cmake_build_path().joinpath(
        "artifacts", *normalized.parts, f"{normalized.name}.elf"
    )


def latest_package(
    build_root: pathlib.Path, examples: tuple[str, ...] | None = None,
) -> pathlib.Path:
    candidates = [
        package_path(build_root, example)
        for example in (examples if examples is not None else example_names())
        if package_path(build_root, example).is_file()
    ]
    if not candidates:
        raise RuntimeError(f"no built GMP packages found in {build_root}")
    return max(
        candidates,
        key=lambda candidate: (candidate.stat().st_mtime_ns, candidate.as_posix()),
    )


def cmake_build_path() -> pathlib.Path:
    system, architecture = host()
    if system == "windows":
        local = os.environ.get("LOCALAPPDATA")
        cache_root = (
            pathlib.Path(local) / "GMPluginSDK" / "cmake"
            if local else pathlib.Path.home() / ".gmpluginsdk" / "cmake"
        )
        # CMake/Ninja invokes cmd.exe for custom rules. cmd.exe cannot use a
        # UNC share as its current directory, so Windows build internals must
        # stay local even when the SDK itself is on a shared drive.
        checkout_id = hashlib.sha256(str(SDK).encode("utf-8")).hexdigest()[:16]
        return cache_root / f"{architecture}-{checkout_id}"
    return BUILD_INTERNALS / f"{system}-{architecture}"


def configure_cmake(
    cmake: pathlib.Path, ninja: pathlib.Path, build_root: pathlib.Path,
    bin_dir: pathlib.Path,
) -> None:
    cmake_build = cmake_build_path()
    command = [
        cmake, "-S", BUILD_DEFINITION, "-B", cmake_build, "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        "-DCMAKE_SYSTEM_NAME=Generic",
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
        "-DCMAKE_C_COMPILER_WORKS=TRUE",
        "-DCMAKE_C_COMPILER_FORCED=TRUE",
        f"-DCMAKE_C_COMPILER={tool(bin_dir, 'gcc')}",
        f"-DCMAKE_AR={tool(bin_dir, 'ar')}",
        f"-DCMAKE_RANLIB={tool(bin_dir, 'ranlib')}",
        f"-DCMAKE_LINKER={tool(bin_dir, 'ld')}",
        f"-DGM_PYTHON_EXECUTABLE={sys.executable}",
        f"-DGM_OUTPUT_ROOT={build_root}",
    ]
    try:
        run(command)
    except subprocess.CalledProcessError:
        # CMake caches absolute source paths. A copied SDK or a mapped network
        # drive can change that spelling, so recreate only this host's cache.
        shutil.rmtree(cmake_build, ignore_errors=True)
        print("Recreating the platform-specific CMake cache...")
        run(command)


def build_cmake(
    cmake: pathlib.Path, build_root: pathlib.Path, target: str | None = None,
) -> None:
    command: list[object] = [cmake, "--build", cmake_build_path(), "--parallel"]
    if target is not None:
        command.extend(("--target", target))
    run(command)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build GM RISC-V plugins on Windows, macOS, or Linux")
    parser.add_argument("command", nargs="?", choices=("build", "all", "clean", "inspect", "toolchain", "serve"), default="serve")
    parser.add_argument("--example", help="example path below examples/")
    parser.add_argument("--build-dir", type=pathlib.Path, default=BUILD_INTERNALS)
    parser.add_argument("--insecure-download", action="store_true", help="disable TLS certificate checks; the pinned SHA-256 is still verified")
    parser.add_argument("--host", help="LAN address encoded in the QR code (auto-detected by default)")
    parser.add_argument("--port", type=int, default=18765, help="TCP port for serve (default: 18765; use 0 for a free port)")
    parser.add_argument("--qr-output", type=pathlib.Path, help="QR PNG path for serve (default: next to the GMP)")
    parser.add_argument("--gmp", type=pathlib.Path, help="serve an existing GMP without rebuilding")
    args = parser.parse_args()
    build_root = args.build_dir.expanduser().resolve()
    if args.command == "clean":
        shutil.rmtree(cmake_build_path(), ignore_errors=True)
        print("Removed temporary build files; prebuilt GMP packages were kept")
        return 0
    if args.command == "serve" and args.gmp is not None:
        gmp = args.gmp.expanduser().resolve()
    else:
        if args.command == "toolchain":
            bin_dir = ensure_toolchain(args.insecure_download)
            run([tool(bin_dir, "gcc"), "--version"])
            return 0
        cmake, ninja = ensure_build_tools()
        bin_dir = ensure_toolchain(args.insecure_download)
        configure_cmake(cmake, ninja, build_root, bin_dir)
        if args.command == "all":
            build_cmake(cmake, build_root)
            return 0
        if args.example is not None:
            example = args.example.replace("\\", "/").strip("/")
            build_cmake(cmake, build_root, cmake_target(example))
            gmp = package_path(build_root, example)
        elif args.command == "serve":
            build_cmake(cmake, build_root)
            gmp = latest_package(build_root)
            print(f"Using the most recently updated package: {gmp}")
        else:
            example = "game/breakout"
            build_cmake(cmake, build_root, cmake_target(example))
            gmp = package_path(build_root, example)
    if args.command == "serve":
        advertised_host = args.host or choose_lan_address()
        qr_output = (args.qr_output or gmp.with_suffix(".qr.png")).expanduser().resolve()
        server, metadata = start_server(gmp, advertised_host, args.port, qr_output)
        print(f"Serving {gmp}")
        print(f"Address: {metadata.host}:{metadata.port}")
        print(f"QR code: {qr_output}")
        print(qr_terminal_text(metadata.to_qr_payload()))
        print("Scan the QR code in Aphrodite. Press Ctrl+C to stop.")
        serve_forever(server)
        return 0
    if args.command == "inspect":
        run([
            tool(bin_dir, "readelf"), "-h", "-l", "-S", "-r", "-s",
            intermediate_elf_path(example),
        ])
    print(f"Built {gmp}")
    return 0


def cli() -> int:
    try:
        return main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"build: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(cli())
