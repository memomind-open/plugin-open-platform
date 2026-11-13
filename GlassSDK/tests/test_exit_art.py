"""Check the shipped ring pixels, including the LVGL 1.0 fallback.

This verifies raster geometry, not the device/Studio compositor.
"""
import math
from pathlib import Path
import re
import unittest

ART = Path(__file__).resolve().parents[1] / 'examples/game/common/firmware_exit_art.h'


def decode(name, side):
    source = ART.read_text(encoding='utf-8')
    body = source.split(f'{name}_pixels[] = {{', 1)[1].split('};', 1)[0]
    data = bytes(int(s, 16) for s in re.findall(r'0x([0-9A-F]{2})', body))
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

    def test_legacy_runs_match_image_pixels(self):
        source = ART.read_text(encoding='utf-8')
        for name, side in [('firmware_exit_icon', 18), ('firmware_exit_half', 48), ('firmware_exit_full', 48)]:
            pixels = decode(name, side)
            body = source.split(f'{name}[] = {{', 1)[1].split('};', 1)[0]
            rows = re.findall(r'\{(\d+), (\d+), (\d+), 0x([0-9A-F]+), (\d+)\}', body)
            raster = [(0, 0)] * (side*side)
            for sx, sy, sw, sg, sa in rows:
                x, y, width, green, alpha = int(sx), int(sy), int(sw), int(sg, 16), int(sa)
                self.assertLessEqual(x+width, side)
                for i in range(width):
                    self.assertEqual(raster[y*side+x+i][1], 0)
                    raster[y*side+x+i] = (green, alpha)
            expected = [(p[1]&0xF0, p[3]) if p[3] else (0,0) for p in pixels]
            self.assertEqual(raster, expected)


if __name__ == '__main__':
    unittest.main()
