"""Compile real display examples and verify their call-UI policies with a mock Host."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[1]
CC = os.environ.get('CC', 'cc')


@unittest.skipUnless(shutil.which(CC), 'native C compiler required')
class SystemEventTests(unittest.TestCase):
    def test_other_examples_yield_display(self):
        defines = {'bluetooth': 'TEST_BLUETOOTH', 'talking_pet': 'TEST_PET',
                   'web_bridge': 'TEST_WEB', 'novel_reader': 'TEST_READER',
                   'imu': 'TEST_IMU', 'framebuffer': 'TEST_FRAMEBUFFER'}
        for source in sorted((SDK / 'examples').rglob('*.c')):
            if source.parent.name in ('fighter_arena', 'extension'):
                continue
            with self.subTest(example=source.parent.name), tempfile.TemporaryDirectory() as folder:
                binary = Path(folder) / 'test'
                flags = [f'-DTEST_PLUGIN_SOURCE="{source.as_posix()}"']
                if source.parent.name in defines:
                    flags.append('-D' + defines[source.parent.name])
                subprocess.run([
                    CC, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    *shlex.split(os.environ.get('CFLAGS', '')), *flags,
                    '-I', str(SDK / 'include'),
                    str(SDK / 'tests' / 'test_examples_call.c'), '-o', str(binary),
                ], check=True)
                subprocess.run([str(binary)], check=True)

    def test_fighter_yields_framebuffer(self):
        with tempfile.TemporaryDirectory(prefix='fighter-call-') as folder:
            binary = Path(folder) / ('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([
                CC, '-std=c11', '-Wall', '-Wextra', '-Werror',
                *shlex.split(os.environ.get('CFLAGS', '')),
                '-I', str(SDK / 'include'),
                str(SDK / 'tests' / 'test_fighter_call.c'),
                '-o', str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
