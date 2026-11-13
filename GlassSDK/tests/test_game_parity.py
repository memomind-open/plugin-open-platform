"""Exercise the three shipped games against a recording Host/LVGL fixture.

These check layout and behavior, not physical display pixels or glyph rasterization.
"""
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
class GameParityTests(unittest.TestCase):
    def test_firmware_layout_input_and_lifecycle(self):
        for game in ('jet_runner', 'breakout', 'tetris'):
            with self.subTest(game=game), tempfile.TemporaryDirectory() as folder:
                binary = Path(folder) / ('test.exe' if os.name == 'nt' else 'test')
                subprocess.run([
                    CC, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    *shlex.split(os.environ.get('CFLAGS', '')),
                    '-DTEST_' + game.upper(), '-I', str(SDK / 'include'),
                    str(SDK / 'tests/test_game_parity.c'), '-o', str(binary),
                ], check=True)
                subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
