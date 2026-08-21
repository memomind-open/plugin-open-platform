"""Insert two generated ground-pound poses into Fighter Arena frame 11.

The input image is a two-character sheet on a green screen.  This keeps the
normal combat sheet at 12 frames, so the RV32 package does not grow merely to
add one special-move pose.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image


FRAME_W = 112
FRAME_H = 98
SHEET_COLS = 4


def extract_pose(source: Image.Image, side: int,
                 target_width: int, target_height: int) -> Image.Image:
    half = source.width // 2
    image = source.crop((side * half, 0, (side + 1) * half, source.height)).convert("RGBA")
    pixels = image.load()
    bounds = [image.width, image.height, -1, -1]
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, _ = pixels[x, y]
            is_green = green > 150 and green > red * 1.35 and green > blue * 1.35
            if is_green:
                pixels[x, y] = (0, 0, 0, 0)
            else:
                gray = round(red * 0.299 + green * 0.587 + blue * 0.114)
                pixels[x, y] = (gray, gray, gray, 255)
                bounds[0] = min(bounds[0], x)
                bounds[1] = min(bounds[1], y)
                bounds[2] = max(bounds[2], x)
                bounds[3] = max(bounds[3], y)
    if bounds[2] < bounds[0]:
        raise ValueError("no foreground sprite found")

    sprite = image.crop((bounds[0], bounds[1], bounds[2] + 1, bounds[3] + 1))
    size = (target_width, target_height)
    sprite = sprite.resize(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (FRAME_W, FRAME_H))
    canvas.alpha_composite(sprite, ((FRAME_W - size[0]) // 2, FRAME_H - size[1]))
    return canvas


def quantized_frame(image: Image.Image) -> list[list[int]]:
    rgba = image.convert("RGBA")
    result: list[list[int]] = []
    for y in range(FRAME_H):
        row: list[int] = []
        for x in range(FRAME_W):
            red, green, blue, alpha = rgba.getpixel((x, y))
            if alpha < 96:
                row.append(0)
            else:
                gray = round((red * 0.299 + green * 0.587 + blue * 0.114) * 14 / 255)
                row.append(max(1, min(14, gray)))
        result.append(row)
    return result


def encode_frame(image: Image.Image) -> bytes:
    encoded = bytearray()
    for row in quantized_frame(image):
        occupied = [index for index, value in enumerate(row) if value]
        if not occupied:
            encoded.extend((0xFF, 0))
            continue
        start, end = occupied[0], occupied[-1]
        values = row[start : end + 1]
        encoded.extend((start, len(values)))
        for index in range(0, len(values), 2):
            high = values[index]
            low = values[index + 1] if index + 1 < len(values) else 0
            encoded.append((high << 4) | low)
    return bytes(encoded)


def replace_header_frame(header: Path, frame_index: int, frame: Image.Image) -> None:
    text = header.read_text(encoding="utf-8")
    offsets_match = re.search(
        r"(static const uint32_t pf_sprite_offsets\[.*?\] = \{\s*)(.*?)(\s*\};)",
        text,
        re.S,
    )
    data_match = re.search(
        r"(static const uint8_t pf_sprite_data\[\] = \{\s*)(.*?)(\s*\};)",
        text,
        re.S,
    )
    if not offsets_match or not data_match:
        raise ValueError(f"unsupported header format: {header}")

    offsets = [int(value) for value in re.findall(r"UINT32_C\((\d+)\)", offsets_match.group(2))]
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})U", data_match.group(2)))
    if len(offsets) != 13 or offsets[-1] != len(data):
        raise ValueError(f"invalid sprite offsets: {header}")

    replacement = encode_frame(frame)
    old_length = offsets[frame_index + 1] - offsets[frame_index]
    data = data[: offsets[frame_index]] + replacement + data[offsets[frame_index + 1] :]
    delta = len(replacement) - old_length
    for index in range(frame_index + 1, len(offsets)):
        offsets[index] += delta
    offset_text = ", ".join(f"UINT32_C({value})" for value in offsets)
    rows = []
    for index in range(0, len(data), 20):
        rows.append("    " + ", ".join(f"0x{value:02X}U" for value in data[index : index + 20]) + ",")
    data_text = "\n".join(rows)
    text = text[: offsets_match.start(2)] + offset_text + text[offsets_match.end(2) :]
    data_match = re.search(
        r"(static const uint8_t pf_sprite_data\[\] = \{\s*)(.*?)(\s*\};)",
        text,
        re.S,
    )
    assert data_match
    text = text[: data_match.start(2)] + data_text + text[data_match.end(2) :]
    header.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("ground_source", type=Path)
    parser.add_argument("grab_source", type=Path)
    parser.add_argument("fighter_dir", type=Path)
    args = parser.parse_args()

    ground_source = Image.open(args.ground_source)
    grab_source = Image.open(args.grab_source)
    for side, name in enumerate(("zen", "rival")):
        ground_pose = extract_pose(ground_source, side, 73, 94)
        grab_pose = extract_pose(grab_source, side, 98, 88)
        ground_pose.save(args.fighter_dir / f"{name}-ground-pound-frame-v1.png")
        grab_pose.save(args.fighter_dir / f"{name}-grapple-frame-v1.png")
        sheet_path = args.fighter_dir / f"{name}-combat-sheet-normalized-v2.png"
        sheet = Image.open(sheet_path).convert("RGBA")
        sheet.alpha_composite(grab_pose, ((10 % SHEET_COLS) * FRAME_W, (10 // SHEET_COLS) * FRAME_H))
        sheet.alpha_composite(ground_pose, ((11 % SHEET_COLS) * FRAME_W, (11 // SHEET_COLS) * FRAME_H))
        sheet.save(args.fighter_dir / f"{name}-combat-sheet-normalized-v3.png")
        header = args.fighter_dir / f"{name}_combat_sprites.h"
        replace_header_frame(header, 10, grab_pose)
        replace_header_frame(header, 11, ground_pose)


if __name__ == "__main__":
    main()
