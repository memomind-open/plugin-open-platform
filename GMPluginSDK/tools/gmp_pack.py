#!/usr/bin/env python3
"""Validate a GM zero-base PIC ELF and package it as an GMP file."""

from __future__ import annotations

import argparse
import binascii
import json
import pathlib
import struct
import sys

ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
SECTION_HEADER = struct.Struct("<IIIIIIIIII")
SYMBOL = struct.Struct("<IIIBBH")
RELA = struct.Struct("<IIi")
# PUBLISHED GMP v1 little-endian header. After release, add a new format packer
# instead of changing this layout and keep the v1 parser in firmware.
GMP_HEADER = struct.Struct("<4sHHIIIII")
GMP_PACKAGE_CRC_OFFSET = GMP_HEADER.size - 4
MAX_PACKAGE_SIZE = 200 * 1024

PT_LOAD = 1
SHT_SYMTAB = 2
SHT_RELA = 4
SHF_ALLOC = 0x2
EM_RISCV = 243
ET_EXEC = 2
SHN_UNDEF = 0

# These relocations are PC-relative or linker-only hints and remain valid when
# the complete image is moved by a common base. Absolute, GOT and TLS forms are
# intentionally rejected.
R_RISCV_32 = 1
ALLOWED_RELOCATIONS = {
    0, R_RISCV_32,
    16, 17, 18, 19,       # BRANCH, JAL, CALL, CALL_PLT
    23, 24, 25,           # PCREL_HI20, PCREL_LO12_I, PCREL_LO12_S
    44, 45, 51,           # RVC_BRANCH, RVC_JUMP, RELAX
}
class PackageError(RuntimeError):
    pass


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    if end < 0:
        end = len(data)
    return data[offset:end].decode("utf-8", errors="replace")


def parse_elf(path: pathlib.Path) -> tuple[bytes, int, int, int, list[int]]:
    data = path.read_bytes()
    if len(data) < ELF_HEADER.size:
        raise PackageError("ELF header is truncated")

    fields = ELF_HEADER.unpack_from(data)
    ident = fields[0]
    e_type = fields[1]
    e_machine = fields[2]
    e_entry = fields[4]
    e_phoff = fields[5]
    e_shoff = fields[6]
    e_phentsize, e_phnum = fields[9], fields[10]
    e_shentsize, e_shnum, e_shstrndx = fields[11:14]
    if ident[:4] != b"\x7fELF" or ident[4] != 1 or ident[5] != 1:
        raise PackageError("only little-endian ELF32 is supported")
    if e_type != ET_EXEC or e_machine != EM_RISCV:
        raise PackageError("plugin must be a RISC-V ELF32 executable")
    if e_phentsize != PROGRAM_HEADER.size or e_shentsize != SECTION_HEADER.size:
        raise PackageError("unexpected ELF table layout")

    sections = [
        SECTION_HEADER.unpack_from(data, e_shoff + i * e_shentsize)
        for i in range(e_shnum)
    ]
    if e_shstrndx >= len(sections):
        raise PackageError("invalid section name table")
    shstr = sections[e_shstrndx]
    names = data[shstr[4]:shstr[4] + shstr[5]]

    base_relocations: list[int] = []
    for index, section in enumerate(sections):
        name = c_string(names, section[0]) if section[0] < len(names) else "?"
        section_type, flags, offset, size, link, entsize = (
            section[1], section[2], section[4], section[5], section[6], section[9]
        )
        if section_type == SHT_RELA and (flags & SHF_ALLOC or name.startswith(".rela")):
            if entsize != RELA.size:
                raise PackageError(f"unexpected relocation size in {name}")
            for pos in range(offset, offset + size, entsize):
                reloc_offset, info, _ = RELA.unpack_from(data, pos)
                relocation = info & 0xFF
                if relocation not in ALLOWED_RELOCATIONS:
                    raise PackageError(
                        f"forbidden relocation type {relocation} in {name}; "
                        "remove absolute pointers, GOT and TLS usage"
                    )
                if relocation == R_RISCV_32:
                    base_relocations.append(reloc_offset)
        if section_type == SHT_SYMTAB:
            if link >= len(sections) or entsize != SYMBOL.size:
                raise PackageError("invalid symbol table")
            strings_section = sections[link]
            strings = data[
                strings_section[4]:strings_section[4] + strings_section[5]
            ]
            for pos in range(offset, offset + size, entsize):
                name_offset, _, _, info, _, shndx = SYMBOL.unpack_from(data, pos)
                binding = info >> 4
                if shndx == SHN_UNDEF and binding != 0:
                    symbol_name = c_string(strings, name_offset)
                    raise PackageError(f"undefined symbol is forbidden: {symbol_name}")

    loads = []
    for i in range(e_phnum):
        ph = PROGRAM_HEADER.unpack_from(data, e_phoff + i * e_phentsize)
        if ph[0] == PT_LOAD:
            loads.append(ph)
    if len(loads) != 1:
        raise PackageError("plugin must contain exactly one PT_LOAD segment")
    _, file_offset, vaddr, _, file_size, memory_size, _, _ = loads[0]
    if (vaddr != 0 or e_entry >= file_size or (e_entry & 1) != 0 or
            file_size > memory_size):
        raise PackageError(
            "plugin image must start at VMA 0 with an aligned entry in its "
            "file-backed image")
    if file_offset + file_size > len(data):
        raise PackageError("PT_LOAD segment is truncated")
    image = data[file_offset:file_offset + file_size]
    base_relocations = sorted(set(base_relocations))
    if any(offset + 4 > file_size or (offset & 3) != 0
           for offset in base_relocations):
        raise PackageError("base relocation target is outside the load image")
    if any(struct.unpack_from("<I", image, offset)[0] >= memory_size
           for offset in base_relocations):
        raise PackageError("base relocation value is outside plugin memory")
    return image, memory_size, e_entry, file_size, base_relocations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True, type=pathlib.Path)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    manifest_object = json.loads(args.manifest.read_text(encoding="utf-8"))
    if not isinstance(manifest_object, dict):
        raise PackageError("manifest root must be an object")
    required = ("id", "name", "version", "abi_version")
    missing = [key for key in required if key not in manifest_object]
    if missing:
        raise PackageError("manifest missing: " + ", ".join(missing))
    duplicated_runtime_fields = {"permissions"}.intersection(manifest_object)
    if duplicated_runtime_fields:
        raise PackageError(
            "manifest must not duplicate runtime descriptor fields: " +
            ", ".join(sorted(duplicated_runtime_fields)))
    if not isinstance(manifest_object["id"], str) or not manifest_object["id"]:
        raise PackageError("manifest id must be a non-empty string")
    if not isinstance(manifest_object["name"], str) or not manifest_object["name"]:
        raise PackageError("manifest name must be a non-empty string")
    if (isinstance(manifest_object["version"], bool) or
            not isinstance(manifest_object["version"], int) or
            manifest_object["version"] < 1):
        raise PackageError("manifest version must be a positive integer")
    abi_version = manifest_object["abi_version"]
    if (isinstance(abi_version, bool) or not isinstance(abi_version, int) or
            not 0 <= abi_version <= 0xFFFF):
        raise PackageError("manifest abi_version must fit uint16")
    image, memory_size, entry_offset, image_size, base_relocations = parse_elf(args.elf)
    header_size = GMP_HEADER.size
    image_offset = header_size
    relocation_data = b"".join(struct.pack("<I", offset)
                               for offset in base_relocations)
    image_end = image_offset + image_size
    relocation_offset = align_up(image_end, 4) if relocation_data else image_end
    image_padding = bytes(relocation_offset - image_end)
    payload_size = relocation_offset + len(relocation_data)
    # The Host turns the received package allocation into the runtime image in
    # place. Keep enough tail capacity for BSS without a second allocation.
    total_size = max(payload_size, memory_size)
    if total_size > MAX_PACKAGE_SIZE:
        raise PackageError(
            f"GMP needs {total_size} bytes; platform limit is "
            f"{MAX_PACKAGE_SIZE} bytes")
    values = (
        b"GMPK", 1, abi_version,
        image_size, memory_size, entry_offset, len(base_relocations), 0,
    )
    header = bytearray(GMP_HEADER.pack(*values))
    package = bytearray(header + image + image_padding + relocation_data)
    package.extend(bytes(total_size - len(package)))
    package_crc = binascii.crc32(package) & 0xFFFFFFFF
    struct.pack_into("<I", package, GMP_PACKAGE_CRC_OFFSET, package_crc)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(package)
    print(
        f"packed {args.output}: image={image_size} bytes, "
        f"memory={memory_size} bytes, relocs={len(base_relocations)}, "
        f"total={total_size} bytes"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, TypeError, ValueError, struct.error, PackageError,
            json.JSONDecodeError) as error:
        print(f"gmp_pack: {error}", file=sys.stderr)
        raise SystemExit(2)
