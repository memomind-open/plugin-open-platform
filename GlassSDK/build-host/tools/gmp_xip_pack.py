#!/usr/bin/env python3
"""GMP v2: position-independent Flash code/constants, separately allocated RAM.

Relocations and address-slot bindings
precede the image, allowing installation with bounded streaming buffers.
"""
from __future__ import annotations
import argparse
import binascii
import json
import pathlib
import struct
import sys
from gmp_elf import (ELF_HEADER, PROGRAM_HEADER, SECTION_HEADER, SYMBOL, RELA,
                      PackageError, validate_provided_protocols)

HEADER = struct.Struct('<4sHH12I64s')
FIXUP = struct.Struct('<IHBB')
POINTER = struct.Struct('<II')
RAM_VMA = 0x10000000
RAM_TAG = 0x80000000
ALIGNMENT = 64
MAX_SLOTS = 128
MAX_FIXUPS = 4096
MAX_FLASH = 500 * 1024
MAX_RAM = 100 * 1024 - 1
MAX_PACKAGE = 3 * 1024 * 1024 - 65536
CRC_OFFSET = 48


def region(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise PackageError('truncated ELF range')
    return data[offset:offset + size]


def table(data, fmt, offset, count, stride):
    if stride != fmt.size:
        raise PackageError('invalid ELF table stride')
    block = region(data, offset, count * stride)
    return [fmt.unpack_from(block, i * stride) for i in range(count)]


def parse(path: pathlib.Path):
    data = path.read_bytes()
    h = ELF_HEADER.unpack(region(data, 0, ELF_HEADER.size))
    if h[0][:7] != b'\x7fELF\x01\x01\x01' or h[1:3] != (2, 243):
        raise PackageError('expected little-endian RISC-V ELF32 executable')
    sections = table(data, SECTION_HEADER, h[6], h[12], h[11])
    for section in sections:
        if section[2] & 2 and section[5] and section[8] > ALIGNMENT:
            raise PackageError('section alignment exceeds 64-byte XIP alignment')
    loads = [p for p in table(data, PROGRAM_HEADER, h[5], h[10], h[9]) if p[0] == 1 and (p[4] or p[5])]
    flash = [p for p in loads if p[2] == 0]
    ram = [p for p in loads if p[2] == RAM_VMA]
    if len(flash) != 1 or len(ram) > 1 or len(loads) != len(flash) + len(ram):
        raise PackageError('expected separate Flash and RAM load segments')
    f = flash[0]
    if f[4] != f[5] or f[6] != 5 or h[4] >= f[4] or h[4] & 1:
        raise PackageError('invalid Flash segment or entry')
    if f[4] > MAX_FLASH:
        raise PackageError(f"Flash code/constants {f[4]} bytes exceed {MAX_FLASH} bytes (500 KiB)")
    image = bytearray(region(data, f[1], f[4]))
    initial = b''
    ram_size = 0
    if ram:
        r = ram[0]
        if r[4] > r[5] or r[6] != 6:
            raise PackageError('invalid RAM segment')
        if r[5] > MAX_RAM:
            raise PackageError(f"static RAM {r[5]} bytes must be below 102400 bytes (100 KiB); malloc is not included")
        initial = region(data, r[1], r[4])
        ram_size = r[5]
    if len(image) % 4 or len(initial) % 4 or ram_size % 4:
        raise PackageError('segments must be word aligned')

    def target(address, allow_end=False):
        if RAM_VMA <= address < RAM_VMA + ram_size + int(allow_end):
            return RAM_TAG | (address - RAM_VMA)
        if 0 <= address < len(image) + int(allow_end):
            return address
        raise PackageError(f'relocation address outside image: {address:#x}')

    symbols = {}
    function_entries = set()
    for index, section in enumerate(sections):
        if section[1] == 2:
            if section[6] >= len(sections):
                raise PackageError('invalid symbol string table')
            syms = table(data, SYMBOL, section[4], section[5] // SYMBOL.size, section[9])
            if section[5] % SYMBOL.size:
                raise PackageError('partial symbol')
            for symbol in syms:
                if symbol[5] == 0 and symbol[3] >> 4:
                    raise PackageError('undefined external symbol')
                if symbol[3] & 15 == 2:
                    function_entries.add(symbol[1])
            symbols[index] = syms
    relocations = []
    for section in sections:
        if section[1] != 4:
            continue
        if section[7] >= len(sections):
            raise PackageError('invalid relocation target section')
        if not sections[section[7]][2] & 2:
            continue
        if section[6] not in symbols or section[5] % RELA.size:
            raise PackageError('invalid relocation symbol table')
        syms = symbols[section[6]]
        for address, info, addend in table(data, RELA, section[4], section[5] // RELA.size, section[9]):
            symbol_index = info >> 8
            if symbol_index >= len(syms):
                raise PackageError('invalid relocation symbol')
            relocations.append((address, info & 255, syms[symbol_index][1] + addend))
    lows = {}
    references = function_entries.copy()
    for address, kind, value in relocations:
        if kind in (24, 25):
            lows.setdefault(value, []).append((address, kind))
        elif kind in (1, 16, 17, 18, 19, 44, 45):
            references.add(value)
    slots = []
    fixups = []
    pointers = []
    used_lows = set()
    for address, kind, value in relocations:
        if kind == 1:
            offset = address - RAM_VMA
            if offset < 0 or offset & 3 or offset + 4 > len(initial):
                raise PackageError('absolute pointer in Flash: use PIC and RAM .data.rel.ro')
            pointers.append((offset, target(value, True)))
        elif kind in (20, 23): # GOT_HI20 or PCREL_HI20
            if address < 0 or address + 8 > len(image) or address & 1:
                raise PackageError('address materialization outside Flash')
            binding = target(value, True)
            if kind == 23 and not binding & RAM_TAG:
                continue # Flash-to-Flash PC-relative address stays unchanged.
            pair = lows.get(address, [])
            if pair != [(address + 4, 24)] or address + 4 in references:
                raise PackageError('RAM address must have one adjacent LO12_I, without an entry into its tail')
            hi, lo = struct.unpack_from('<II', image, address)
            rd = (hi >> 7) & 31
            expected = 0x2003 if kind == 20 else 0x13
            if (hi & 0x7f != 0x17 or rd == 0 or lo & 0x707f != expected or
                    (lo >> 7) & 31 != rd or (lo >> 15) & 31 != rd):
                raise PackageError('unsupported cross-segment instruction pair')
            if binding not in slots:
                slots.append(binding)
            slot = slots.index(binding)
            fixups.append((address, slot, rd, 0))
            used_lows.add(address + 4)
            struct.pack_into('<II', image, address, (rd << 7) | 0x37,
                             (rd << 15) | (rd << 7) | 0x2003)
        elif kind in (24, 25):
            pass # Every cross-segment HI above validates its unique low pair.
        elif kind in (16, 17, 18, 19, 44, 45):
            if not 0 <= value < len(image):
                raise PackageError('control-flow target outside Flash')
        elif kind not in (0, 51):
            raise PackageError(f'unsupported relocation {kind}')
    fixups.sort()
    pointers.sort()
    if len(slots) > MAX_SLOTS or len(fixups) > MAX_FIXUPS:
        raise PackageError('XIP address-table limit exceeded')
    if any(a[0] + 8 > b[0] for a, b in zip(fixups, fixups[1:])):
        raise PackageError('overlapping address pairs')
    if any(a[0] == b[0] for a, b in zip(pointers, pointers[1:])):
        raise PackageError('duplicate RAM pointer relocation')
    return bytes(image), initial, ram_size, h[4], slots, fixups, pointers


def pack(elf: pathlib.Path, manifest: dict) -> bytes:
    if (not isinstance(manifest, dict) or
            not isinstance(manifest.get('id'), str) or not manifest['id'] or
            not isinstance(manifest.get('name'), str) or not manifest['name'] or '\0' in manifest['name'] or
            len(manifest['name'].encode('utf-8')) > 63 or
            type(manifest.get('version')) is not int or not 0 < manifest['version'] <= 0xffffffff or
            type(manifest.get('abi_version')) is not int or not 0 <= manifest['abi_version'] <= 0xffff):
        raise PackageError('invalid plugin identity')
    if 'permissions' in manifest:
        raise PackageError('manifest must not duplicate runtime permissions')
    validate_provided_protocols(manifest)
    image, initial, ram_size, entry, slots, fixups, pointers = parse(elf)
    metadata = (b''.join(struct.pack('<I', value) for value in slots) +
                b''.join(FIXUP.pack(*value) for value in fixups) +
                b''.join(POINTER.pack(*value) for value in pointers))
    flash_offset = (HEADER.size + len(metadata) + ALIGNMENT - 1) & -ALIGNMENT
    metadata += bytes(flash_offset - HEADER.size - len(metadata))
    ram_offset = flash_offset + len(image)
    size = ram_offset + len(initial)
    if size > MAX_PACKAGE:
        raise PackageError('package exceeds CUS8 capacity')
    header = HEADER.pack(b'GMPK', 2, manifest['abi_version'], size, len(image),
                         len(initial), ram_size, entry, len(slots), len(fixups),
                         len(pointers), flash_offset, ram_offset, 0, manifest['version'],
                         manifest['name'].encode('utf-8'))
    package = bytearray(header + metadata + image + initial)
    struct.pack_into('<I', package, CRC_OFFSET, binascii.crc32(package) & 0xffffffff)
    return bytes(package)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--elf', required=True, type=pathlib.Path)
    parser.add_argument('--manifest', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    args = parser.parse_args()
    package = pack(args.elf, json.loads(args.manifest.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(package)
    header = HEADER.unpack_from(package)
    print(f'packed XIP {args.output}: Flash={header[4]}/{MAX_FLASH} B; '
          f'static RAM={header[6]} B (<102400 B, excludes heap/stack/Host overhead); '
          f'slots={header[8]} total={len(package)} B')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TypeError, struct.error, PackageError) as error:
        print(f'gmp_xip_pack: {error}', file=sys.stderr)
        sys.exit(2)
