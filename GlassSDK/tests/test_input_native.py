"""Native input tests. Set CC to a native C compiler; optional CFLAGS for sanitizers."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[1]
CC = os.environ.get('CC', 'cc')


@unittest.skipUnless(shutil.which(CC), 'native C compiler required (set CC)')
class InputTests(unittest.TestCase):
    def run_fixture(self, name):
        with tempfile.TemporaryDirectory(prefix='gm-input-') as folder:
            binary = Path(folder) / ('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([CC, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            *shlex.split(os.environ.get('CFLAGS', '')),
                            '-I', str(SDK / 'include'), str(SDK / 'tests' / name),
                            '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_breakout(self):
        self.run_fixture('test_input_breakout.c')


if __name__ == '__main__':
    unittest.main()
