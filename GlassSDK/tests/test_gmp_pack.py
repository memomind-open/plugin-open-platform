from __future__ import annotations

import unittest

from tools.gmp_pack import PackageError, validate_provided_protocols


class GmpPackProtocolTest(unittest.TestCase):
    def test_accepts_versioned_protocol_declarations(self) -> None:
        validate_provided_protocols({
            "provides": {
                "protocols": [
                    {"id": "gm.scene", "version": "1.0"},
                    {"id": "gm.scene-atomic-frame", "version": "1.0.1"},
                ]
            }
        })

    def test_rejects_duplicate_or_invalid_protocols(self) -> None:
        with self.assertRaises(PackageError):
            validate_provided_protocols({
                "provides": {
                    "protocols": [
                        {"id": "gm.scene", "version": "1.0"},
                        {"id": "gm.scene", "version": "2.0"},
                    ]
                }
            })
        with self.assertRaises(PackageError):
            validate_provided_protocols({
                "provides": {
                    "protocols": [{"id": "GM Scene", "version": "latest"}]
                }
            })


if __name__ == "__main__":
    unittest.main()
