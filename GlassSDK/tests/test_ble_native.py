"""Public BLE ABI, optional lookup, and capability-only example verification."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[1]
CC = os.environ.get("CC", "cc")


class BleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which(CC), "native C compiler required")
    def test_native_contract_and_example(self):
        with tempfile.TemporaryDirectory(prefix="gm-ble-sdk-") as directory:
            binary = Path(directory) / ("test.exe" if os.name == "nt" else "test")
            subprocess.run([CC, "-std=c11", "-Wall", "-Wextra", "-Werror",
                            *shlex.split(os.environ.get("CFLAGS", "")),
                            "-I", str(SDK / "include"),
                            str(SDK / "tests/test_ble_native.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    @unittest.skipUnless(os.environ.get("GM_FIRMWARE_INCLUDE_DIR"),
                         "set GM_FIRMWARE_INCLUDE_DIR for cross-repository parity")
    def test_firmware_ble_contract_matches(self):
        firmware = Path(os.environ["GM_FIRMWARE_INCLUDE_DIR"])
        self.assertEqual((SDK / "include/gm_plugin_ble_gatt.h").read_bytes(),
                         (firmware / "gm_plugin_ble_gatt.h").read_bytes())
        pattern = (r"typedef struct gm_plugin_ble_gatt_extension_api\s*\{.*?"
                   r"\}\s*gm_plugin_ble_gatt_extension_api_t;")
        sdk_header = (SDK / "include/gm_plugin_extensions.h").read_text()
        firmware_header = (firmware / "gm_plugin_extensions.h").read_text()
        self.assertEqual(re.search(pattern, sdk_header, re.S).group(),
                         re.search(pattern, firmware_header, re.S).group())
        identity = r"#define GM_PLUGIN_EXTENSION_BLE_GATT UINT32_C\(6\)"
        self.assertRegex(sdk_header, identity)
        self.assertRegex(firmware_header, identity)
        refresh = (r"typedef struct gm_plugin_ble_gatt_refresh_extension_api\s*\{.*?"
                   r"\}\s*gm_plugin_ble_gatt_refresh_extension_api_t;")
        self.assertEqual(re.search(refresh, sdk_header, re.S).group(),
                         re.search(refresh, firmware_header, re.S).group())
        bond_list = (r"typedef struct gm_plugin_ble_gatt_bond_list_extension_api\s*\{.*?"
                     r"\}\s*gm_plugin_ble_gatt_bond_list_extension_api_t;")
        self.assertEqual(re.search(bond_list, sdk_header, re.S).group(),
                         re.search(bond_list, firmware_header, re.S).group())
        for header in (sdk_header, firmware_header):
            self.assertRegex(header, r"#define GM_PLUGIN_EXTENSION_BLE_GATT_REFRESH UINT32_C\(7\)")
            self.assertRegex(header, r"#define GM_PLUGIN_EXTENSION_BLE_GATT_BOND_LIST UINT32_C\(8\)")


if __name__ == "__main__":
    unittest.main()
