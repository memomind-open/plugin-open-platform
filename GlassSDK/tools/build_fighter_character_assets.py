"""Normalize generated Fighter Arena sheets and encode runtime sprite headers.

The built-in image generator may bake its transparency preview checkerboard into
an RGB image.  This tool removes only border-connected light neutral pixels,
then fits every generated pose into the corresponding bounds of the existing
normalized sheet.  Reusing those bounds keeps animation scale and collision
spacing stable when a character design is replaced.
"""

from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path
import shutil

from PIL import Image


FRAME_W = 112
FRAME_H = 98


def remove_preview_background(source: Image.Image) -> Image.Image:
    rgba = source.convert("RGBA")
    rgb = rgba.convert("RGB")
    width, height = rgba.size
    pixels = rgb.load()
    source_alpha = rgba.getchannel("A")
    background = bytearray(width * height)
    pending: deque[tuple[int, int]] = deque()

    def is_preview_background(x: int, y: int) -> bool:
        red, green, blue = pixels[x, y]
        return min(red, green, blue) >= 225 and max(red, green, blue) - min(red, green, blue) <= 12

    def enqueue(x: int, y: int) -> None:
        index = y * width + x
        if not background[index] and is_preview_background(x, y):
            background[index] = 1
            pending.append((x, y))

    for x in range(width):
        enqueue(x, 0)
        enqueue(x, height - 1)
    for y in range(height):
        enqueue(0, y)
        enqueue(width - 1, y)

    while pending:
        x, y = pending.popleft()
        if x:
            enqueue(x - 1, y)
        if x + 1 < width:
            enqueue(x + 1, y)
        if y:
            enqueue(x, y - 1)
        if y + 1 < height:
            enqueue(x, y + 1)

    result = rgba.copy()
    alpha = Image.new("L", (width, height), 255)
    alpha.putdata([
        0 if background[index] else value
        for index, value in enumerate(source_alpha.getdata())
    ])
    result.putalpha(alpha)
    return result


def cell_box(size: tuple[int, int], cols: int, rows: int,
             column: int, row: int) -> tuple[int, int, int, int]:
    width, height = size
    return (
        round(column * width / cols),
        round(row * height / rows),
        round((column + 1) * width / cols),
        round((row + 1) * height / rows),
    )


def visible_bounds(image: Image.Image) -> tuple[int, int, int, int]:
    alpha = image.convert("RGBA").getchannel("A")
    bounds = alpha.point(lambda value: 255 if value >= 96 else 0).getbbox()
    if bounds is None:
        raise ValueError("sprite cell has no visible pixels")
    return bounds


def remove_small_components(image: Image.Image) -> Image.Image:
    """Remove detached cell leaks while retaining the complete fighter body."""
    image = image.convert("RGBA")
    alpha = image.getchannel("A")
    width, height = image.size
    occupied = bytearray(1 if value >= 96 else 0 for value in alpha.getdata())
    visited = bytearray(width * height)
    components: list[list[int]] = []

    for start in range(width * height):
        if not occupied[start] or visited[start]:
            continue
        visited[start] = 1
        pending = deque([start])
        component: list[int] = []
        while pending:
            index = pending.popleft()
            component.append(index)
            x = index % width
            y = index // width
            for neighbor_y in range(max(0, y - 1), min(height, y + 2)):
                for neighbor_x in range(max(0, x - 1), min(width, x + 2)):
                    neighbor = neighbor_y * width + neighbor_x
                    if occupied[neighbor] and not visited[neighbor]:
                        visited[neighbor] = 1
                        pending.append(neighbor)
        components.append(component)

    if not components:
        raise ValueError("sprite cell has no visible pixels")
    minimum_size = max(len(component) for component in components) // 20
    values = list(alpha.getdata())
    for component in components:
        if len(component) < minimum_size:
            for index in component:
                values[index] = 0
    alpha.putdata(values)
    image.putalpha(alpha)
    return image


def normalize_sheet(source: Image.Image, reference: Image.Image,
                    cols: int, rows: int) -> Image.Image:
    source = source.convert("RGBA")
    reference = reference.convert("RGBA")
    output = Image.new("RGBA", reference.size)
    target_cell_w = reference.width // cols
    target_cell_h = reference.height // rows

    for index in range(cols * rows):
        column = index % cols
        row = index // cols
        src_cell = source.crop(cell_box(source.size, cols, rows, column, row))
        src_cell = remove_small_components(src_cell)
        src_bounds = visible_bounds(src_cell)
        sprite = src_cell.crop(src_bounds)

        ref_cell = reference.crop((
            column * target_cell_w,
            row * target_cell_h,
            (column + 1) * target_cell_w,
            (row + 1) * target_cell_h,
        ))
        left, top, right, bottom = visible_bounds(ref_cell)
        target_w = right - left
        target_h = bottom - top
        scale = min(target_w / sprite.width, target_h / sprite.height)
        new_size = (
            max(1, round(sprite.width * scale)),
            max(1, round(sprite.height * scale)),
        )
        sprite = sprite.resize(new_size, Image.Resampling.LANCZOS)
        x = column * target_cell_w + left + (target_w - new_size[0]) // 2
        y = row * target_cell_h + bottom - new_size[1]
        output.alpha_composite(sprite, (x, y))
    return output


def quantized_frame(image: Image.Image) -> list[list[int]]:
    rgba = image.convert("RGBA")
    result: list[list[int]] = []
    for y in range(rgba.height):
        row: list[int] = []
        for x in range(rgba.width):
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
        values = row[start:end + 1]
        encoded.extend((start, len(values)))
        for index in range(0, len(values), 2):
            high = values[index]
            low = values[index + 1] if index + 1 < len(values) else 0
            encoded.append((high << 4) | low)
    return bytes(encoded)


def write_header(path: Path, sheet: Image.Image, cols: int, rows: int) -> None:
    frame_w = sheet.width // cols
    frame_h = sheet.height // rows
    frames: list[bytes] = []
    offsets = [0]
    for index in range(cols * rows):
        column = index % cols
        row = index // cols
        frame = sheet.crop((
            column * frame_w,
            row * frame_h,
            (column + 1) * frame_w,
            (row + 1) * frame_h,
        ))
        frames.append(encode_frame(frame))
        offsets.append(offsets[-1] + len(frames[-1]))

    data = b"".join(frames)
    offset_text = ", ".join(f"UINT32_C({value})" for value in offsets)
    data_rows = []
    for index in range(0, len(data), 20):
        values = ", ".join(f"0x{value:02X}U" for value in data[index:index + 20])
        data_rows.append(f"    {values},")
    text = f"""#ifndef PIXEL_FIGHTER_SPRITES_H
#define PIXEL_FIGHTER_SPRITES_H

#include <stdint.h>

#define PF_SPRITE_WIDTH {frame_w}U
#define PF_SPRITE_HEIGHT {frame_h}U
#define PF_SPRITE_FRAME_COUNT {cols * rows}U

static const uint32_t pf_sprite_offsets[PF_SPRITE_FRAME_COUNT + 1U] = {{
    {offset_text}
}};

static const uint8_t pf_sprite_data[] = {{
{chr(10).join(data_rows)}
}};

#endif
"""
    path.write_text(text, encoding="utf-8")


def process_sheet(source_path: Path, reference_path: Path,
                  alpha_path: Path, normalized_paths: list[Path],
                  header_path: Path, cols: int, rows: int) -> Image.Image:
    cleaned = remove_preview_background(Image.open(source_path))
    cleaned.save(alpha_path)
    normalized = normalize_sheet(cleaned, Image.open(reference_path), cols, rows)
    for path in normalized_paths:
        normalized.save(path)
    write_header(header_path, normalized, cols, rows)
    return normalized


def replace_green_screen_character(path: Path, source_cell: Image.Image,
                                   side: int) -> None:
    sheet = Image.open(path).convert("RGBA")
    half = sheet.width // 2
    side_image = sheet.crop((side * half, 0, (side + 1) * half, sheet.height))
    bounds = [side_image.width, side_image.height, -1, -1]
    for y in range(side_image.height):
        for x in range(side_image.width):
            red, green, blue, _ = side_image.getpixel((x, y))
            is_green = green > 150 and green > red * 1.35 and green > blue * 1.35
            if not is_green:
                bounds[0] = min(bounds[0], x)
                bounds[1] = min(bounds[1], y)
                bounds[2] = max(bounds[2], x)
                bounds[3] = max(bounds[3], y)
    if bounds[2] < bounds[0]:
        raise ValueError(f"no right-side character in {path}")

    green = sheet.getpixel((sheet.width - 1, 0))
    replacement = Image.new("RGBA", (half, sheet.height), green)
    source_cell = remove_small_components(source_cell)
    source_cell = source_cell.crop(visible_bounds(source_cell))
    target_w = bounds[2] - bounds[0] + 1
    target_h = bounds[3] - bounds[1] + 1
    scale = min(target_w / source_cell.width, target_h / source_cell.height)
    size = (max(1, round(source_cell.width * scale)), max(1, round(source_cell.height * scale)))
    source_cell = source_cell.resize(size, Image.Resampling.LANCZOS)
    x = bounds[0] + (target_w - size[0]) // 2
    y = bounds[3] + 1 - size[1]
    replacement.alpha_composite(source_cell, (x, y))
    sheet.paste(replacement, (side * half, 0))
    sheet.convert("RGB").save(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("combat_source", type=Path)
    parser.add_argument("hurt_source", type=Path)
    parser.add_argument("fighter_dir", type=Path)
    parser.add_argument("--character", choices=("zen", "rival"), default="rival")
    parser.add_argument("--reference-dir", type=Path)
    args = parser.parse_args()

    fighter_dir = args.fighter_dir
    reference_dir = args.reference_dir or fighter_dir
    character = args.character
    combat_reference = reference_dir / f"{character}-combat-sheet-normalized-v3.png"
    hurt_reference = reference_dir / f"{character}-hurt-sheet-normalized-v1.png"

    shutil.copyfile(args.combat_source, fighter_dir / f"{character}-combat-sheet-generated-v1.png")
    shutil.copyfile(args.hurt_source, fighter_dir / f"{character}-hurt-sheet-generated-v1.png")

    combat = process_sheet(
        args.combat_source,
        combat_reference,
        fighter_dir / f"{character}-combat-sheet-alpha-v1.png",
        [
            fighter_dir / f"{character}-combat-sheet-normalized-v2.png",
            fighter_dir / f"{character}-combat-sheet-normalized-v3.png",
        ],
        fighter_dir / f"{character}_combat_sprites.h",
        4,
        3,
    )
    hurt = process_sheet(
        args.hurt_source,
        hurt_reference,
        fighter_dir / f"{character}-hurt-sheet-alpha-v1.png",
        [fighter_dir / f"{character}-hurt-sheet-normalized-v1.png"],
        fighter_dir / f"{character}_hurt_sprites.h",
        2,
        2,
    )

    cleaned_combat = Image.open(
        fighter_dir / f"{character}-combat-sheet-alpha-v1.png"
    ).convert("RGBA")
    legacy_combat_reference = Image.open(
        reference_dir / f"{character}-combat-sheet-normalized-v1.png"
    )
    legacy_combat = normalize_sheet(cleaned_combat, legacy_combat_reference, 4, 3)
    legacy_combat.save(fighter_dir / f"{character}-combat-sheet-normalized-v1.png")

    if character == "rival":
        first_eight_bottom = round(cleaned_combat.height * 2 / 3)
        first_eight = cleaned_combat.crop((0, 0, cleaned_combat.width, first_eight_bottom))
        raw_first_eight = Image.open(args.combat_source).crop(
            (0, 0, cleaned_combat.width, first_eight_bottom)
        )
        raw_first_eight.save(fighter_dir / "rival-sheet-generated-v1.png")
        first_eight.save(fighter_dir / "rival-sheet-alpha-v1.png")
        legacy_reference = Image.open(reference_dir / "rival-sheet-normalized-v1.png")
        legacy = normalize_sheet(first_eight, legacy_reference, 4, 2)
        legacy.save(fighter_dir / "rival-sheet-normalized-v1.png")
        write_header(fighter_dir / "rival_sprites.h", legacy, 4, 2)

    combat.crop((2 * FRAME_W, 2 * FRAME_H, 3 * FRAME_W, 3 * FRAME_H)).save(
        fighter_dir / f"{character}-grapple-frame-v1.png"
    )
    combat.crop((3 * FRAME_W, 2 * FRAME_H, 4 * FRAME_W, 3 * FRAME_H)).save(
        fighter_dir / f"{character}-ground-pound-frame-v1.png"
    )

    replace_green_screen_character(
        fighter_dir / "grapple-generated-v1.png",
        cleaned_combat.crop(cell_box(cleaned_combat.size, 4, 3, 2, 2)),
        0 if character == "zen" else 1,
    )
    replace_green_screen_character(
        fighter_dir / "ground-pound-generated-v1.png",
        cleaned_combat.crop(cell_box(cleaned_combat.size, 4, 3, 3, 2)),
        0 if character == "zen" else 1,
    )

    print(f"combat frames: 12, normalized: {combat.size}")
    print(f"hurt frames: 4, normalized: {hurt.size}")


if __name__ == "__main__":
    main()
