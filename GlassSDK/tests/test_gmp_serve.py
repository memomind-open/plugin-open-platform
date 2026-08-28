from __future__ import annotations

import hashlib
import pathlib
import socket
import tempfile
import unittest
from unittest import mock

from tools import gmp_serve
from tools.gmp_serve import (
    REQUEST_LINE,
    PackageServer,
    package_metadata,
    qr_terminal_text,
    serve_in_thread,
)


class GmpServeTest(unittest.TestCase):
    def test_missing_qr_dependency_is_installed_with_current_python(self) -> None:
        qr_code = mock.Mock()
        qr_module = mock.Mock(QrCode=qr_code)
        with mock.patch("tools.gmp_serve.subprocess.run") as run, mock.patch(
            "tools.gmp_serve.importlib.import_module",
            side_effect=[ModuleNotFoundError, qr_module],
        ) as import_module:
            self.assertIs(gmp_serve._load_qr_code(), qr_code)

        import_module.assert_has_calls([mock.call("qrcodegen"), mock.call("qrcodegen")])
        run.assert_called_once_with(
            [
                gmp_serve.sys.executable,
                "-m",
                "pip",
                "install",
                "-r",
                str(gmp_serve.SDK_ROOT / "requirements.txt"),
            ],
            check=True,
        )

    def test_qr_payload_contains_download_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = pathlib.Path(directory) / "snake.gmp"
            package.write_bytes(b"GMPK" + bytes(28))
            metadata = package_metadata(package, "192.168.1.8", 18765)
            self.assertEqual(
                metadata.to_qr_payload(),
                "gmp+tcp://192.168.1.8:18765/snake.gmp",
            )

            original_payload = metadata.to_qr_payload()
            package.write_bytes(b"GMPK" + bytes(64))
            self.assertEqual(metadata.to_qr_payload(), original_payload)

    def test_qr_uses_low_error_correction_for_screen_scanning(self) -> None:
        qr_code = mock.Mock()
        qr_code.Ecc.LOW = mock.sentinel.low
        with mock.patch.object(gmp_serve, "_load_qr_code", return_value=qr_code):
            gmp_serve._encode_qr("gmp+tcp://192.168.1.8:18765/snake.gmp")

        qr_code.encode_text.assert_called_once_with(
            "gmp+tcp://192.168.1.8:18765/snake.gmp",
            mock.sentinel.low,
        )

    def test_server_sends_exact_package_after_valid_request(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = pathlib.Path(directory) / "snake.gmp"
            expected = b"GMPK" + bytes(range(256)) * 4
            package.write_bytes(expected)
            server = PackageServer("127.0.0.1", 0, package)
            thread = serve_in_thread(server)
            try:
                with socket.create_connection(server.server_address, timeout=2) as client:
                    client.sendall(REQUEST_LINE)
                    response = client.makefile("rb")
                    checksum = hashlib.sha256(expected).hexdigest()
                    self.assertEqual(
                        response.readline(),
                        f"GMP/1 OK {len(expected)} {checksum}\n".encode(),
                    )
                    self.assertEqual(response.read(), expected)
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_server_serves_a_rebuilt_package_without_changing_the_qr(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = pathlib.Path(directory) / "snake.gmp"
            package.write_bytes(b"GMPK" + bytes(28))
            metadata = package_metadata(package, "192.168.1.8", 18765)
            qr_payload = metadata.to_qr_payload()
            server = PackageServer("127.0.0.1", 0, package)
            thread = serve_in_thread(server)
            try:
                rebuilt = b"GMPK" + bytes(range(64))
                package.write_bytes(rebuilt)
                with socket.create_connection(server.server_address, timeout=2) as client:
                    client.sendall(REQUEST_LINE)
                    response = client.makefile("rb")
                    checksum = hashlib.sha256(rebuilt).hexdigest()
                    self.assertEqual(
                        response.readline(),
                        f"GMP/1 OK {len(rebuilt)} {checksum}\n".encode(),
                    )
                    self.assertEqual(response.read(), rebuilt)
                self.assertEqual(metadata.to_qr_payload(), qr_payload)
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_terminal_qr_uses_compact_half_block_rendering(self) -> None:
        rendered = qr_terminal_text('{"v":1}', half_blocks=True)
        self.assertIn("▀", rendered)
        self.assertIn("\033[30m", rendered)
        self.assertIn("\033[37m", rendered)
        self.assertIn("\033[40m", rendered)
        self.assertIn("\033[47m", rendered)
        self.assertTrue(rendered.endswith("\033[0m"))
        self.assertLessEqual(len(rendered.splitlines()), 20)

    def test_terminal_qr_has_a_windows_safe_background_fallback(self) -> None:
        with mock.patch.object(gmp_serve.os, "name", "nt"):
            rendered = qr_terminal_text('{"v":1}')
        self.assertNotIn("▀", rendered)
        self.assertIn("\033[40m", rendered)
        self.assertIn("\033[47m", rendered)
        self.assertIn("  ", rendered)
        self.assertTrue(rendered.endswith("\033[0m"))

    def test_server_rejects_unknown_request(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = pathlib.Path(directory) / "snake.gmp"
            package.write_bytes(b"GMPK" + bytes(28))
            server = PackageServer("127.0.0.1", 0, package)
            thread = serve_in_thread(server)
            try:
                with socket.create_connection(server.server_address, timeout=2) as client:
                    client.sendall(b"GET / HTTP/1.1\n")
                    self.assertEqual(client.recv(128), b"GMP/1 ERROR invalid-request\n")
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_metadata_rejects_non_gmp_and_invalid_size_or_magic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            cases = {
                "plugin.bin": b"GMPK" + bytes(28),
                "tiny.gmp": b"GMPK",
                "bad-magic.gmp": b"NOPE" + bytes(28),
                "bad\nname.gmp": b"GMPK" + bytes(28),
            }
            for name, content in cases.items():
                with self.subTest(name=name):
                    package = root / name
                    package.write_bytes(content)
                    with self.assertRaises(RuntimeError):
                        package_metadata(package, "192.168.1.8", 18765)

    def test_metadata_rejects_an_unusable_host_or_port(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = pathlib.Path(directory) / "snake.gmp"
            package.write_bytes(b"GMPK" + bytes(28))
            for host, port in (
                ("http://192.168.1.8", 18765),
                ("bad host", 18765),
                ("192.168.1.8", -1),
                ("192.168.1.8", 65536),
            ):
                with self.subTest(host=host, port=port):
                    with self.assertRaises(RuntimeError):
                        package_metadata(package, host, port)


if __name__ == "__main__":
    unittest.main()
