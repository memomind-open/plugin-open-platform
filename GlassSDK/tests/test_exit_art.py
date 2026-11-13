"""Check the shipped LVGL API 1.1 images used by the hold-exit UI.

This verifies raster geometry, not the device/Studio compositor.
"""
import hashlib
import math
from pathlib import Path
import re
import unittest

ART = Path(__file__).resolve().parents[1] / 'examples/game/common/firmware_exit_art.h'


def image_bytes(name):
    source = ART.read_text(encoding='utf-8')
    body = source.split(f'{name}_pixels[] = {{', 1)[1].split('};', 1)[0]
    return bytes(int(s, 16) for s in re.findall(r'0x([0-9A-F]{2})', body))


def decode(name, side):
    data = image_bytes(name)
    assert len(data) == 64 + side * side // 2
    pixels = []
    for byte in data[64:]:
        for index in (byte >> 4, byte & 15):
            b, g, r, a = data[4*index:4*index+4]
            pixels.append((r, g, b, a))
    return pixels


class ExitArtTests(unittest.TestCase):
    def test_circle_radius_stroke_and_symmetry(self):
        full = decode('firmware_exit_full', 48)
        for y in range(48):
            for x in range(48):
                alpha = full[y*48+x][3]
                self.assertEqual(alpha, full[y*48+47-x][3])
                self.assertEqual(alpha, full[(47-y)*48+x][3])
                self.assertEqual(alpha, full[x*48+y][3])
                radius = math.hypot(x+0.5-24, y+0.5-24)
                if radius < 19.25 or radius > 24.75:
                    self.assertEqual(alpha, 0)
                if 20.75 < radius < 23.25:
                    self.assertEqual(alpha, 255)
        # Octagonal chords cut inside the circular centerline between vertices.
        for degree in range(360):
            angle = math.radians(degree)
            x = int(24 + 22*math.cos(angle))
            y = int(24 + 22*math.sin(angle))
            self.assertEqual(full[y*48+x][3], 255)
        self.assertGreater(len({p[3] for p in full}), 8)

    def test_half_is_clockwise_with_rounded_endcaps(self):
        half = decode('firmware_exit_half', 48)
        full = decode('firmware_exit_full', 48)
        for y in range(48):
            for x in range(48):
                self.assertEqual(half[y*48+x][3], half[(47-y)*48+x][3])
                if x >= 26:
                    self.assertEqual(half[y*48+x], full[y*48+x])
                if x < 22:
                    self.assertEqual(half[y*48+x][3], 0)
        self.assertEqual(half[2*48+23][3], 255)  # Rounded top cap.
        self.assertEqual(half[24*48+46][3], 255)
        self.assertEqual(half[24*48+1][3], 0)

    def test_image_payloads_preserve_shipped_art(self):
        # Keep the exact palettes and pixels when removing the object fallback.
        expected = {
            'firmware_exit_icon': '1ab0239a0fcaa04594ab95ffdc4c04a58e5b418c25f9b08ba78bb8d9b2d62626',
            'firmware_exit_half': '37b0ac362f142264d2a1830c5158614ef6efe243143431f0cdf01772a039fc9a',
            'firmware_exit_full': '6800e53977ca3ac03bc102bda29374af27a145d5d84402f63e4e913cab3df368',
        }
        for name, digest in expected.items():
            with self.subTest(image=name):
                self.assertEqual(hashlib.sha256(image_bytes(name)).hexdigest(), digest)


if __name__ == '__main__':
    unittest.main()
