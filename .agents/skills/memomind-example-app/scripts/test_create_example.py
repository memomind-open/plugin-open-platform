"""Behavior checks for the starter generator; temporary projects only, no SDK rebuild."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).with_name('create_example.py')
REPO = SCRIPT.resolve().parents[4]


@unittest.skipUnless(shutil.which('node'), 'Node 18+ is required for Web generation')
class StarterTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='memomind-generator-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def run_generator(self, *arguments, success=True):
        result = subprocess.run([sys.executable, str(SCRIPT), *arguments], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def create(self, kind, title='New example'):
        target = self.root / (kind + ' with spaces')
        self.run_generator('--kind', kind, '--name', 'new-example', '--id', 'com.example.new',
                           '--title', title, '--output', str(target))
        return target

    def test_web_package_has_entry_sdk_and_new_identity(self):
        project = self.create('web', 'A <new> "example"')
        package = self.root / 'new.mmpkg'
        subprocess.run(['node', str(REPO / 'PhoneSDK/tools/build-mmpkg.mjs'), str(project), str(package)],
                       check=True, capture_output=True, text=True)
        with zipfile.ZipFile(package) as archive:
            manifest = json.loads(archive.read('manifest.json'))
            self.assertEqual(manifest['id'], 'com.example.new')
            self.assertIn(manifest['entry'], archive.namelist())
            self.assertIn('vendor/gm-plugin-web-sdk.esm.js', archive.namelist())
            self.assertIn('createGMPlugin', archive.read('vendor/gm-plugin-web-sdk.esm.js').decode())
            self.assertIn('A &lt;new&gt; &quot;example&quot;', archive.read('index.html').decode())

    def test_generation_does_not_depend_on_locale_encoding(self):
        target = self.root / 'ascii-locale'
        environment = dict(os.environ, LC_ALL='C', PYTHONUTF8='0', PYTHONCOERCECLOCALE='0')
        result = subprocess.run(
            [sys.executable, str(SCRIPT), '--kind', 'web', '--name', 'new-example',
             '--id', 'com.example.new', '--title', 'Example', '--output', str(target)],
            env=environment, capture_output=True, text=True, encoding='utf-8',
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('正在连接宿主', (target / 'index.html').read_text(encoding='utf-8'))
        self.assertIn('createGMPlugin', (target / 'vendor/gm-plugin-web-sdk.esm.js').read_text(encoding='utf-8'))

    def test_existing_directory_and_user_files_are_preserved(self):
        project = self.create('web')
        marker = project / 'user.txt'
        marker.write_text('keep this')
        manifest = (project / 'manifest.json').read_bytes()
        self.run_generator('--kind', 'web', '--name', 'new-example', '--id', 'com.example.other',
                           '--title', 'Other', '--output', str(project), success=False)
        self.assertEqual(marker.read_text(), 'keep this')
        self.assertEqual((project / 'manifest.json').read_bytes(), manifest)

    def test_bad_directory_slug_cannot_escape_examples(self):
        target = self.root / 'escaped'
        self.run_generator('--kind', 'web', '--name', '../escaped', '--id', 'com.example.new',
                           '--title', 'Other', '--output', str(target), success=False)
        self.assertFalse(target.exists())

    def test_refresh_only_changes_vendor(self):
        project = self.create('web')
        before = {p.name: p.read_bytes() for p in project.iterdir() if p.is_file()}
        sdk = project / 'vendor/gm-plugin-web-sdk.esm.js'
        sdk.write_text('old bundle')
        self.run_generator('--refresh-sdk', str(project))
        self.assertIn('createGMPlugin', sdk.read_text())
        self.assertEqual(before, {p.name: p.read_bytes() for p in project.iterdir() if p.is_file()})

    def test_refresh_refuses_vendor_symlink(self):
        project = self.create('web')
        protected = self.root / 'outside.js'
        protected.write_text('do not overwrite')
        sdk = project / 'vendor/gm-plugin-web-sdk.esm.js'
        sdk.unlink()
        sdk.symlink_to(protected)
        self.run_generator('--refresh-sdk', str(project), success=False)
        self.assertEqual(protected.read_text(), 'do not overwrite')

    def test_glass_shared_header_is_local_and_buildable(self):
        project = self.create('glass')
        self.assertTrue((project / 'call_ui.h').exists())
        manifest = json.loads((project / 'manifest.json').read_text())
        self.assertIsInstance(manifest['version'], int)
        if shutil.which('cc'):
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
                            '-I', str(REPO / 'GlassSDK/include'), str(project / 'new-example.c')], check=True)


if __name__ == '__main__':
    unittest.main()
