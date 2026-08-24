from __future__ import annotations

import os
import pathlib
import tempfile
import unittest
from unittest import mock

from tools.gm_build import cmake_build_path, cmake_target, latest_package, package_path


class CMakeBuildDriverTest(unittest.TestCase):
    def test_latest_package_selects_the_most_recent_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            older = package_path(root, "minimal")
            newer = package_path(root, "game/snake")
            older.parent.mkdir(parents=True)
            newer.parent.mkdir(parents=True)
            older.write_bytes(b"older")
            newer.write_bytes(b"newer")
            os.utime(older, ns=(1_000_000_000, 1_000_000_000))
            os.utime(newer, ns=(2_000_000_000, 2_000_000_000))
            self.assertEqual(
                latest_package(root, ("minimal", "game/snake")), newer
            )
            os.utime(older, ns=(2_000_000_000, 2_000_000_000))
            self.assertEqual(
                latest_package(root, ("game/snake", "minimal")), older
            )

    def test_latest_package_rejects_an_empty_build(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "no built GMP packages"):
                latest_package(pathlib.Path(directory), ("minimal",))

    def test_cmake_target_uses_a_stable_nested_example_name(self) -> None:
        self.assertEqual(cmake_target("game/snake"), "gm_plugin_game_snake")
        self.assertEqual(cmake_target("game\\snake"), "gm_plugin_game_snake")

    def test_package_path_matches_the_public_output_layout(self) -> None:
        root = pathlib.Path("build-host")
        self.assertEqual(
            package_path(root, "game/snake"),
            root / "game" / "snake" / "snake.gmp",
        )
        self.assertEqual(
            package_path(root, "game\\snake"),
            root / "game" / "snake" / "snake.gmp",
        )

    def test_cmake_cache_is_nested_below_the_public_output_root(self) -> None:
        cache = cmake_build_path(pathlib.Path("build-host"))
        self.assertEqual(cache.parts[:2], ("build-host", ".cmake"))

    def test_windows_cmake_cache_uses_a_local_directory(self) -> None:
        with mock.patch("tools.gm_build.host", return_value=("windows", "x64")), \
             mock.patch.dict("tools.gm_build.os.environ", {"LOCALAPPDATA": "C:\\Users\\sdk"}):
            cache = cmake_build_path(pathlib.Path(r"Y:\\GMPluginSDK\\build-host"))
        self.assertTrue(str(cache).startswith(r"C:\Users\sdk"))
        self.assertNotIn("build-host", str(cache))


if __name__ == "__main__":
    unittest.main()
