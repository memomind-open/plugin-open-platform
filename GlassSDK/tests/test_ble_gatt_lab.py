"""Execute the actual BLE lab against a deterministic Host and display fixture."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[1]
CC = os.environ.get("CC", "cc")


@unittest.skipUnless(shutil.which(CC), "native C compiler required")
class BleGattLabTests(unittest.TestCase):
    def test_lab_operations_and_lifecycle(self):
        with tempfile.TemporaryDirectory(prefix="gm-ble-gatt-lab-") as directory:
            binary = Path(directory) / ("lab.exe" if os.name == "nt" else "lab")
            subprocess.run([CC, "-std=c11", "-Wall", "-Wextra", "-Werror",
                            *shlex.split(os.environ.get("CFLAGS", "")),
                            "-I", str(SDK / "include"),
                            str(SDK / "tests/test_ble_gatt_lab.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
