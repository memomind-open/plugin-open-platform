"""Pairing regressions using synthetic manifests/header metadata, not executable GMP fixtures."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
from check_pair import HEADER, check_gmp, check_pair, read_manifest

WEB = {'id': 'com.example.remote', 'deviceRequirements': {
    'preferredPluginId': 'com.example.pet',
    'protocols': [{'id': 'gm.example-pet', 'minVersion': '1.2'}],
}}
GLASS = {'id': 'com.example.pet', 'name': 'Example Pet', 'version': 3, 'abi_version': 256,
         'provides': {'protocols': [{'id': 'gm.example-pet', 'version': '1.2.0'}]}}


class PairTests(unittest.TestCase):
    def test_matching_protocol_accepts_equivalent_numeric_versions(self):
        self.assertEqual(check_pair(WEB, GLASS), ([], []))

    def test_default_web_bridge_does_not_provide_custom_protocol(self):
        bridge = {'id': 'com.gm.example.web-bridge', 'provides': {
            'protocols': [{'id': 'gm.scene', 'version': '1.0'}]}}
        errors, notes = check_pair(WEB, bridge)
        self.assertIn('Missing protocol gm.example-pet', errors)
        self.assertTrue(notes)

    def test_preferred_identity_is_advisory_but_required_identity_is_enforced(self):
        alternative = copy.deepcopy(GLASS)
        alternative['id'] = 'com.example.alternative'
        errors, notes = check_pair(WEB, alternative)
        self.assertFalse(errors)
        self.assertTrue(notes)
        web = copy.deepcopy(WEB)
        web['deviceRequirements']['requiredPluginId'] = GLASS['id']
        self.assertTrue(check_pair(web, alternative)[0])

    def test_protocol_and_required_package_minimum_versions(self):
        old = copy.deepcopy(GLASS)
        old['provides']['protocols'][0]['version'] = '1.1.9'
        self.assertIn('Protocol version too old: gm.example-pet', check_pair(WEB, old)[0])
        web = copy.deepcopy(WEB)
        web['deviceRequirements'].update(requiredPluginId=GLASS['id'], minPluginVersion='4')
        self.assertIn('Glass package version is below minPluginVersion', check_pair(web, GLASS)[0])

    def test_malformed_or_duplicate_protocols_do_not_pass(self):
        invalid = copy.deepcopy(GLASS)
        invalid['provides']['protocols'][0]['version'] = 'latest'
        with self.assertRaises(ValueError):
            check_pair(WEB, invalid)
        invalid['provides']['protocols'] = GLASS['provides']['protocols'] * 2
        with self.assertRaises(ValueError):
            check_pair(WEB, invalid)
        with self.assertRaises(ValueError):
            check_pair({'id': 'com.example.phone-only'}, GLASS)

    def test_built_web_package_requirements_are_used(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'old.mmpkg'
            stale = copy.deepcopy(WEB)
            stale['deviceRequirements']['protocols'][0]['minVersion'] = '2.0'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('manifest.json', json.dumps(stale))
            self.assertTrue(check_pair(read_manifest(path), GLASS)[0])

    def test_gmp_presence_and_identity_metadata(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'pet.gmp'
            # Header-only fixture: this checker intentionally does not validate executable code.
            path.write_bytes(HEADER.pack(b'GMPK', 2, 256, HEADER.size,
                                         *([0] * 10), 3, b'Example Pet'))
            check_gmp(path, GLASS)
            wrong = dict(GLASS, name='Different Plugin')
            with self.assertRaises(ValueError):
                check_gmp(path, wrong)
            path.write_bytes(b'GMPK')
            with self.assertRaises(ValueError):
                check_gmp(path, GLASS)
            path.unlink()
            with self.assertRaises(FileNotFoundError):
                check_gmp(path, GLASS)

    def test_cli_returns_failure_for_incompatible_pair(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'web.json').write_text(json.dumps(WEB), encoding='utf-8')
            (root / 'glass.json').write_text(json.dumps({'id': 'com.example.unrelated'}), encoding='utf-8')
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('check_pair.py')),
                                     '--web', str(root / 'web.json'), '--glass', str(root / 'glass.json')],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn('Missing protocol gm.example-pet', result.stdout)


if __name__ == '__main__':
    unittest.main()
