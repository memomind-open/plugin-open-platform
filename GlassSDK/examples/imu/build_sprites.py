#!/usr/bin/env python3
"""Build display-calibrated GRAY4 RLE head-pose sprites."""

import argparse
from pathlib import Path
from typing import List

from PIL import Image, ImageEnhance, ImageFilter, ImageOps


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "assets" / "head-motion-sprites.png"
OUTPUT = ROOT / "head_sprites.h"
FRAME_WIDTH = 200
FRAME_HEIGHT = 186
GRID_COLUMNS = 3
GRID_ROWS = 3
ALPHA_CUTOFF = 24


def quantize(frame: Image.Image) -> List[int]:
    """Convert one RGBA portrait to transparent-background GRAY4 values."""
    alpha = frame.getchannel("A")
    gray = ImageOps.grayscale(frame)
    gray = ImageEnhance.Contrast(gray).enhance(1.08)
    gray = gray.filter(ImageFilter.UnsharpMask(radius=1, percent=110, threshold=3))
    alpha_pixels = alpha.load()
    gray_pixels = gray.load()
    values = [0] * (FRAME_WIDTH * FRAME_HEIGHT)
    for y in range(FRAME_HEIGHT):
        for x in range(FRAME_WIDTH):
            opacity = alpha_pixels[x, y]
            if opacity < ALPHA_CUTOFF:
                continue
            luminance = gray_pixels[x, y] * opacity // 255
            level = (luminance * 15 + 127) // 255
            values[y * FRAME_WIDTH + x] = max(1, level)
    return values


def encode(values: List[int]) -> bytes:
    output = bytearray()
    start = 0
    while start < len(values):
        value = values[start]
        count = 1
        while (
            start + count < len(values)
            and values[start + count] == value
            and count < 255
        ):
            count += 1
        output.extend((count, value))
        start += count
    return bytes(output)


def preview_frame(values: List[int]) -> Image.Image:
    preview = Image.new("RGB", (FRAME_WIDTH, FRAME_HEIGHT), "black")
    pixels = preview.load()
    for index, value in enumerate(values):
        shade = value * 17
        pixels[index % FRAME_WIDTH, index // FRAME_WIDTH] = (
            shade,
            shade,
            shade,
        )
    return preview


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--preview",
        type=Path,
        help="optional path for a black-background 3x3 preview PNG",
    )
    arguments = parser.parse_args()
    sheet = Image.open(SOURCE).convert("RGBA")
    frames = []
    previews = []
    for row in range(GRID_ROWS):
        for column in range(GRID_COLUMNS):
            left = round(column * sheet.width / GRID_COLUMNS)
            right = round((column + 1) * sheet.width / GRID_COLUMNS)
            top = round(row * sheet.height / GRID_ROWS)
            bottom = round((row + 1) * sheet.height / GRID_ROWS)
            cell = sheet.crop((left, top, right, bottom))
            cell = cell.resize(
                (FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS
            )
            values = quantize(cell)
            frames.append(encode(values))
            previews.append(preview_frame(values))

    if arguments.preview:
        montage = Image.new(
            "RGB",
            (FRAME_WIDTH * GRID_COLUMNS, FRAME_HEIGHT * GRID_ROWS),
            "black",
        )
        for index, preview in enumerate(previews):
            montage.paste(
                preview,
                (
                    (index % GRID_COLUMNS) * FRAME_WIDTH,
                    (index // GRID_COLUMNS) * FRAME_HEIGHT,
                ),
            )
        arguments.preview.parent.mkdir(parents=True, exist_ok=True)
        montage.save(arguments.preview)

    lines = [
        "#ifndef HEAD_SPRITES_H",
        "#define HEAD_SPRITES_H",
        "",
        f"#define HEAD_SPRITE_WIDTH {FRAME_WIDTH}U",
        f"#define HEAD_SPRITE_HEIGHT {FRAME_HEIGHT}U",
        f"#define HEAD_SPRITE_COUNT {len(frames)}U",
        "",
    ]
    for index, data in enumerate(frames):
        lines.append(f"static const uint8_t head_sprite_{index}[] = {{")
        for offset in range(0, len(data), 20):
            chunk = ", ".join(
                f"0x{value:02X}" for value in data[offset : offset + 20]
            )
            lines.append(f"    {chunk},")
        lines.append("};")
        lines.append("")
    lines.append("static const uint8_t *const head_sprites[HEAD_SPRITE_COUNT] = {")
    lines.append(
        "    " + ", ".join(f"head_sprite_{index}" for index in range(len(frames))) + ","
    )
    lines.append("};")
    lines.append("static const uint32_t head_sprite_sizes[HEAD_SPRITE_COUNT] = {")
    lines.append(
        "    " + ", ".join(f"sizeof(head_sprite_{index})" for index in range(len(frames))) + ","
    )
    lines.append("};")
    lines.extend(("", "#endif"))
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"wrote {OUTPUT}: {sum(map(len, frames))} RLE bytes")
    if arguments.preview:
        print(f"wrote preview {arguments.preview}")


if __name__ == "__main__":
    main()
