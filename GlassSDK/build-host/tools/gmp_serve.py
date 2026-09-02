#!/usr/bin/env python3
"""Serve one GMP inside the unified developer-app ZIP protocol."""

from __future__ import annotations

import hashlib
import io
import importlib
import os
import pathlib
import socket
import socketserver
import struct
import subprocess
import sys
import threading
import zlib
import zipfile
from dataclasses import dataclass
from urllib.parse import quote


PROTOCOL = "mmapp+tcp"
REQUEST_PREFIX = "MMAPP/1 GET"
MAX_REQUEST_BYTES = 192
MIN_PACKAGE_BYTES = 28
MAX_PACKAGE_BYTES = 200 * 1024
GMP_HEADER = struct.Struct("<4sHHIIIII")
GMP_PACKAGE_CRC_OFFSET = GMP_HEADER.size - 4
SDK_ROOT = pathlib.Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class PackageMetadata:
    host: str
    port: int
    name: str

    def to_qr_payload(self) -> str:
        return f"{PROTOCOL}://{self.host}:{self.port}"


def read_package(path: pathlib.Path) -> bytes:
    data = path.read_bytes()
    if len(data) < MIN_PACKAGE_BYTES or len(data) > MAX_PACKAGE_BYTES:
        raise RuntimeError(
            f"GMP package size must be {MIN_PACKAGE_BYTES}..{MAX_PACKAGE_BYTES} "
            f"bytes: {len(data)}"
        )
    (
        magic,
        version,
        _flags,
        image_size,
        memory_size,
        entry_offset,
        relocation_count,
        stored_crc,
    ) = GMP_HEADER.unpack_from(data)
    relocation_offset = GMP_HEADER.size + image_size
    if relocation_count:
        relocation_offset = (relocation_offset + 3) & ~3
    expected_size = max(memory_size, relocation_offset + relocation_count * 4)
    if (
        magic != b"GMPK"
        or version != 1
        or image_size == 0
        or memory_size < image_size
        or entry_offset >= image_size
        or entry_offset & 1
        or expected_size != len(data)
    ):
        raise RuntimeError(f"package has an invalid GMP v1 layout: {path}")
    crc_input = bytearray(data)
    crc_input[GMP_PACKAGE_CRC_OFFSET:GMP_HEADER.size] = b"\0" * 4
    if zlib.crc32(crc_input) & 0xFFFFFFFF != stored_crc:
        raise RuntimeError(f"package failed its GMP v1 CRC32 check: {path}")
    return data


def choose_lan_address() -> str:
    """Return the address selected by the host routing table, without sending."""
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.0.2.1", 9))
        address = probe.getsockname()[0]
        if address and not address.startswith("127."):
            return address
    except OSError:
        pass
    finally:
        probe.close()
    try:
        for address in socket.gethostbyname_ex(socket.gethostname())[2]:
            if address and not address.startswith("127."):
                return address
    except OSError:
        pass
    raise RuntimeError("could not determine a LAN address; pass --host explicitly")


class _PackageRequestHandler(socketserver.BaseRequestHandler):
    def handle(self) -> None:
        self.request.settimeout(5)
        request = bytearray()
        while len(request) < MAX_REQUEST_BYTES and not request.endswith(b"\n"):
            block = self.request.recv(1)
            if not block:
                return
            request.extend(block)
        try:
            parts = bytes(request).decode("ascii").strip().split()
        except UnicodeDecodeError:
            parts = []
        if (
            len(parts) != 4
            or parts[:2] != REQUEST_PREFIX.split()
            or any(
                value != "-"
                and (
                    len(value) != 64
                    or any(c not in "0123456789abcdefABCDEF" for c in value)
                )
                for value in parts[2:]
            )
        ):
            self.request.sendall(b"MMAPP/1 ERROR invalid-request\n")
            return
        package_path: pathlib.Path = self.server.package_path  # type: ignore[attr-defined]
        try:
            package = read_package(package_path)
        except (OSError, RuntimeError):
            self.request.sendall(b"MMAPP/1 ERROR invalid-package\n")
            return
        package_checksum = hashlib.sha256(package).hexdigest()
        bundle_name = quote(self.server.bundle_name, safe="")  # type: ignore[attr-defined]
        if parts[3].lower() == package_checksum:
            self.request.sendall(
                f"MMAPP/1 NOT_MODIFIED B {bundle_name}\n".encode("ascii")
            )
            return
        output = io.BytesIO()
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
            archive.writestr(package_path.name, package)
        bundle = output.getvalue()
        checksum = hashlib.sha256(bundle).hexdigest()
        self.request.sendall(
            f"MMAPP/1 OK {len(bundle)} {checksum} B {bundle_name}\n".encode("ascii")
        )
        self.request.sendall(bundle)


class PackageServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(
        self,
        bind_host: str,
        port: int,
        package_path: pathlib.Path,
        bundle_name: str,
    ):
        self.package_path = package_path
        self.bundle_name = bundle_name
        super().__init__((bind_host, port), _PackageRequestHandler)


def package_metadata(
    package_path: pathlib.Path, advertised_host: str, port: int
) -> PackageMetadata:
    if (
        not advertised_host
        or len(advertised_host) > 253
        or any(character.isspace() or character in "/:" for character in advertised_host)
    ):
        raise RuntimeError(f"invalid advertised host: {advertised_host!r}")
    if port < 0 or port > 65535:
        raise RuntimeError(f"invalid TCP port: {port}")
    package_path = package_path.resolve()
    if not package_path.is_file():
        raise RuntimeError(f"GMP package not found: {package_path}")
    if package_path.suffix.lower() != ".gmp":
        raise RuntimeError(f"package must use the .gmp extension: {package_path}")
    bundle_stem = package_path.stem.strip()
    if not bundle_stem:
        raise RuntimeError(f"package name is empty: {package_path}")
    if len(package_path.name.encode("utf-8")) > 128 or any(
        ord(character) < 32 or ord(character) == 127
        for character in package_path.name
    ) or "\\" in package_path.name:
        raise RuntimeError(f"package has an unsafe file name: {package_path.name!r}")
    bundle_name = f"{bundle_stem}.zip"
    if len(bundle_name.encode("utf-8")) > 128:
        raise RuntimeError(f"bundle file name is too long: {bundle_name!r}")
    read_package(package_path)
    return PackageMetadata(
        host=advertised_host,
        port=port,
        name=bundle_name,
    )


def _load_qr_code():
    try:
        return importlib.import_module("qrcodegen").QrCode
    except ModuleNotFoundError:
        requirements = SDK_ROOT / "build-host" / "tools" / "requirements.txt"
        print("Installing the QR code dependency...")
        try:
            subprocess.run(
                [sys.executable, "-m", "pip", "install", "-r", str(requirements)],
                check=True,
            )
        except subprocess.CalledProcessError as error:
            raise RuntimeError(
                "could not install the QR code dependency; run: "
                f"{sys.executable} -m pip install -r {requirements}"
            ) from error
        try:
            return importlib.import_module("qrcodegen").QrCode
        except ModuleNotFoundError as error:
            raise RuntimeError(
                "qrcodegen is unavailable after installation; run: "
                f"{sys.executable} -m pip install -r {requirements}"
            ) from error


def _encode_qr(payload: str):
    qr_code = _load_qr_code()
    return qr_code.encode_text(payload, qr_code.Ecc.LOW)


def _png_chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def write_qr_png(payload: str, output: pathlib.Path, scale: int = 8) -> None:
    qr = _encode_qr(payload)
    border = 4
    modules = qr.get_size() + border * 2
    width = modules * scale
    rows = bytearray()
    for pixel_y in range(width):
        module_y = pixel_y // scale - border
        rows.append(0)
        for pixel_x in range(width):
            module_x = pixel_x // scale - border
            dark = (
                0 <= module_x < qr.get_size()
                and 0 <= module_y < qr.get_size()
                and qr.get_module(module_x, module_y)
            )
            rows.append(0 if dark else 255)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, width, 8, 0, 0, 0, 0))
        + _png_chunk(b"IDAT", zlib.compress(bytes(rows), 9))
        + _png_chunk(b"IEND", b"")
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(png)


def qr_terminal_text(payload: str, *, half_blocks: bool | None = None) -> str:
    """Render a high-contrast QR code suitable for the current terminal."""
    qr = _encode_qr(payload)
    border = 4
    if half_blocks is None:
        half_blocks = os.name != "nt"
        if half_blocks:
            try:
                "▀".encode(sys.stdout.encoding or "utf-8")
            except (LookupError, UnicodeEncodeError):
                half_blocks = False

    if not half_blocks:
        rows = []
        for module_y in range(-border, qr.get_size() + border):
            row = []
            current_dark = None
            for module_x in range(-border, qr.get_size() + border):
                dark = (
                    0 <= module_x < qr.get_size()
                    and 0 <= module_y < qr.get_size()
                    and qr.get_module(module_x, module_y)
                )
                if dark != current_dark:
                    row.append("\033[40m" if dark else "\033[47m")
                    current_dark = dark
                row.append("  ")
            row.append("\033[0m")
            rows.append("".join(row))
        return "\n".join(rows)

    rows = []

    def is_dark(module_x: int, module_y: int) -> bool:
        return (
            0 <= module_x < qr.get_size()
            and 0 <= module_y < qr.get_size()
            and qr.get_module(module_x, module_y)
        )

    for module_y in range(-border, qr.get_size() + border, 2):
        row = []
        current_top_dark = None
        current_bottom_dark = None
        for module_x in range(-border, qr.get_size() + border):
            top_dark = is_dark(module_x, module_y)
            bottom_dark = is_dark(module_x, module_y + 1)
            if top_dark != current_top_dark:
                row.append("\033[30m" if top_dark else "\033[37m")
                current_top_dark = top_dark
            if bottom_dark != current_bottom_dark:
                row.append("\033[40m" if bottom_dark else "\033[47m")
                current_bottom_dark = bottom_dark
            row.append("▀")
        row.append("\033[0m")
        rows.append("".join(row))
    return "\n".join(rows)


def start_server(
    package_path: pathlib.Path,
    advertised_host: str,
    port: int,
    qr_output: pathlib.Path,
) -> tuple[PackageServer, PackageMetadata]:
    metadata = package_metadata(package_path, advertised_host, port)
    server = PackageServer(
        "0.0.0.0", port, package_path.resolve(), metadata.name
    )
    metadata = PackageMetadata(
        host=metadata.host,
        port=server.server_address[1],
        name=metadata.name,
    )
    try:
        write_qr_png(metadata.to_qr_payload(), qr_output)
    except Exception:
        server.server_close()
        raise
    return server, metadata


def serve_forever(server: PackageServer) -> None:
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


def serve_in_thread(server: PackageServer) -> threading.Thread:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return thread
