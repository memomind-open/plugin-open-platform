#!/usr/bin/env python3
"""Build sparse, display-calibrated GRAY4 RLE sprites for the native example."""

import argparse
from pathlib import Path
from typing import List
from PIL import Image, ImageEnhance, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "assets" / "memo-sprites.png"
OUTPUT = ROOT / "memo_sprites.h"
FRAME_WIDTH = 144
FRAME_HEIGHT = 184
ALPHA_CUTOFF = 40

def percentile(values: List[int], position: float) -> int:
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * position)]


def quantize(frame: Image.Image) -> List[int]:
    """Convert a pose to continuous GRAY4 art with a subdued base fill."""
    alpha = frame.getchannel("A")
    source_gray = ImageOps.grayscale(frame)
    gray = ImageEnhance.Contrast(source_gray).enhance(1.15)
    gray = gray.filter(ImageFilter.UnsharpMask(radius=1, percent=125, threshold=4))
    alpha_pixels = alpha.load()
    source_pixels = source_gray.load()
    gray_pixels = gray.load()
    width, height = frame.size
    values = [0] * (width * height)
    opaque_luminance = [
        gray_pixels[x, y]
        for y in range(height)
        for x in range(width)
        if alpha_pixels[x, y] >= ALPHA_CUTOFF
    ]
    if not opaque_luminance:
        return values
    low = percentile(opaque_luminance, 0.03)
    high = percentile(opaque_luminance, 0.985)
    span = max(1, high - low)

    for y in range(height):
        for x in range(width):
            if alpha_pixels[x, y] < ALPHA_CUTOFF:
                continue
            source_level = source_pixels[x, y]
            normalized = min(1.0, max(0.0, (gray_pixels[x, y] - low) / span))
            normalized = normalized ** 0.95
            if source_level >= 253:
                level = 15
            elif source_level >= 250:
                level = 14
            elif source_level >= 247:
                level = 13
            else:
                # Preserve continuous surfaces without letting the light-gray
                # face glow. Only near-white eyes and highlights use 13-15.
                level = 1 + round(normalized * 5)
            values[y * width + x] = 1 + level
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
    preview = Image.new("RGBA", (FRAME_WIDTH, FRAME_HEIGHT))
    pixels = preview.load()
    for index, value in enumerate(values):
        if value == 0:
            continue
        shade = (value - 1) * 17
        pixels[index % FRAME_WIDTH, index // FRAME_WIDTH] = (
            shade,
            shade,
            shade,
            255,
        )
    return preview


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--preview",
        type=Path,
        help="optional path for a black-background 4x2 preview PNG",
    )
    arguments = parser.parse_args()
    sheet = Image.open(SOURCE).convert("RGBA")
    frames = []
    previews = []
    for row in range(2):
        for column in range(4):
            left = round(column * sheet.width / 4)
            right = round((column + 1) * sheet.width / 4)
            top = round(row * sheet.height / 2)
            bottom = round((row + 1) * sheet.height / 2)
            cell = sheet.crop((left, top, right, bottom))
            bounds = cell.getbbox()
            if bounds:
                cell = cell.crop(bounds)
            cell.thumbnail((FRAME_WIDTH - 4, FRAME_HEIGHT - 4), Image.Resampling.LANCZOS)
            frame = Image.new("RGBA", (FRAME_WIDTH, FRAME_HEIGHT))
            frame.alpha_composite(
                cell,
                ((FRAME_WIDTH - cell.width) // 2, FRAME_HEIGHT - cell.height - 2),
            )
            values = quantize(frame)
            frames.append(encode(values))
            previews.append(preview_frame(values))

    if arguments.preview:
        montage = Image.new(
            "RGBA",
            (FRAME_WIDTH * 4, FRAME_HEIGHT * 2),
            (0, 0, 0, 255),
        )
        for index, preview in enumerate(previews):
            montage.alpha_composite(
                preview,
                ((index % 4) * FRAME_WIDTH, (index // 4) * FRAME_HEIGHT),
            )
        arguments.preview.parent.mkdir(parents=True, exist_ok=True)
        montage.convert("RGB").save(arguments.preview)

    lines = [
        "#ifndef MEMO_SPRITES_H",
        "#define MEMO_SPRITES_H",
        "",
        f"#define MEMO_SPRITE_WIDTH {FRAME_WIDTH}U",
        f"#define MEMO_SPRITE_HEIGHT {FRAME_HEIGHT}U",
        f"#define MEMO_SPRITE_COUNT {len(frames)}U",
        "",
    ]
    for index, data in enumerate(frames):
        lines.append(f"static const uint8_t memo_sprite_{index}[] = {{")
        for offset in range(0, len(data), 20):
            chunk = ", ".join(
                f"0x{value:02X}" for value in data[offset : offset + 20]
            )
            lines.append(f"    {chunk},")
        lines.append("};")
        lines.append("")
    lines.append("static const uint8_t *const memo_sprites[MEMO_SPRITE_COUNT] = {")
    lines.append(
        "    "
        + ", ".join(f"memo_sprite_{index}" for index in range(len(frames)))
        + ","
    )
    lines.append("};")
    lines.append("static const uint32_t memo_sprite_sizes[MEMO_SPRITE_COUNT] = {")
    lines.append(
        "    "
        + ", ".join(f"sizeof(memo_sprite_{index})" for index in range(len(frames)))
        + ","
    )
    lines.append("};")
    lines.extend(("", "#endif"))
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"wrote {OUTPUT}: {sum(map(len, frames))} RLE bytes")
    if arguments.preview:
        print(f"wrote preview {arguments.preview}")


if __name__ == "__main__":
    main()
