"""Compile real RV32 ELF fixtures and validate the GMP v2 packaging boundary."""
import pathlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / 'build-host/tools'
sys.path.insert(0, str(TOOLS))
from gmp_xip_pack import pack, HEADER, PackageError
from xip_compile import normalize

CC = os.environ.get('GM_XIP_TEST_CC') or shutil.which('riscv64-unknown-elf-gcc') or '/opt/riscv-toolchain/10.2.0/bin/riscv64-unknown-elf-gcc'
MANIFEST = {'id': 'test.xip', 'name': 'XIP fixture', 'version': 7, 'abi_version': 256}

class XipPackTest(unittest.TestCase):
    def memory_elf(self, flash_size, ram_size, check_linker=True):
        directory = tempfile.TemporaryDirectory(prefix='gmp-limits-test-')
        self.addCleanup(directory.cleanup)
        root = pathlib.Path(directory.name)
        source = root / 'limits.s'
        source.write_text(f'''
            .section .text.gm_plugin_entry,"ax",@progbits
            .global gm_plugin_entry
            .type gm_plugin_entry,@function
            gm_plugin_entry:
            .word 0x00008067
            .space {flash_size - 4}
            .section .bss,"aw",@nobits
            .space {ram_size}
        ''')
        script = (TOOLS / 'build/gm_plugin_xip.ld').read_text()
        if not check_linker:
            script = '\n'.join(line for line in script.splitlines()
                               if 'ASSERT(__gm_' not in line)
        (root / 'plugin.ld').write_text(script)
        elf = root / 'limits.elf'
        result = subprocess.run([CC, '-march=rv32imac', '-mabi=ilp32', '-nostdlib',
            '-Wl,--no-relax,--emit-relocs', '-T', str(root / 'plugin.ld'),
            str(source), '-o', str(elf)], capture_output=True, text=True)
        return elf, result

    def test_flash_and_static_ram_exact_limits(self):
        elf, result = self.memory_elf(500 * 1024, 100 * 1024 - 4)
        self.assertEqual(result.returncode, 0, result.stderr)
        header = HEADER.unpack_from(pack(elf, MANIFEST))
        self.assertEqual(header[4], 500 * 1024)
        self.assertEqual(header[6], 100 * 1024 - 4)

    def test_linker_rejects_flash_and_ram_overflow(self):
        for flash, ram, diagnostic in [(500 * 1024 + 4, 4, 'Flash'),
                                       (4, 100 * 1024, 'static RAM')]:
            with self.subTest(flash=flash, ram=ram):
                _, result = self.memory_elf(flash, ram)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)

    def test_packer_rejects_limits_even_without_linker_asserts(self):
        for flash, ram in [(500 * 1024 + 4, 4), (4, 100 * 1024)]:
            with self.subTest(flash=flash, ram=ram):
                elf, result = self.memory_elf(flash, ram, check_linker=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                with self.assertRaises(PackageError):
                    pack(elf, MANIFEST)

    def compile(self, source):
        directory = tempfile.TemporaryDirectory(prefix='gmp-xip-test-')
        self.addCleanup(directory.cleanup)
        root = pathlib.Path(directory.name)
        (root/'fixture.c').write_text(source)
        subprocess.run([sys.executable, str(TOOLS/'xip_compile.py'), CC,
            '-march=rv32imac', '-mabi=ilp32', '-mcmodel=medany', '-mno-relax',
            '-fPIC', '-fvisibility=hidden', '-msmall-data-limit=0', '-Os',
            '-fno-jump-tables', '-ffunction-sections', '-fdata-sections',
            '-c', str(root/'fixture.c'), '-o', str(root/'fixture.o')], check=True)
        subprocess.run([CC, '-march=rv32imac', '-mabi=ilp32', '-nostdlib',
            '-Wl,--no-relax,--emit-relocs,--gc-sections',
            '-T', str(TOOLS/'build/gm_plugin_xip.ld'), str(root/'fixture.o'),
            '-o', str(root/'fixture.elf')], check=True)
        return root/'fixture.elf'

    def test_flash_only_ignores_empty_ram_program_header(self):
        data = pack(self.compile('int gm_plugin_entry(void) { return 42; }'), MANIFEST)
        h = HEADER.unpack_from(data)
        self.assertEqual(h[1], 2)
        self.assertEqual(h[5:7], (0, 0))
        self.assertEqual(h[11] % 64, 0)

    def test_alignment_and_cross_segment_addresses(self):
        data = pack(self.compile('''
            static volatile int counter __attribute__((aligned(64))) = 3;
            static const char text[64] __attribute__((aligned(64))) = "constant";
            static const char * volatile pointer = text;
            int gm_plugin_entry(void) { return counter++ + pointer[0]; }
        '''), MANIFEST)
        h = HEADER.unpack_from(data)
        self.assertEqual(h[11] % 64, 0)
        self.assertGreater(h[8], 0)
        self.assertGreater(h[9], 0)
        self.assertGreater(h[10], 0)
        self.assertIn(b'constant', data[h[11]:h[12]])
        self.assertNotIn(b'constant', data[h[12]:])
        slots = struct.unpack_from('<'+'I'*h[8], data, HEADER.size)
        self.assertTrue(any(value & 0x80000000 for value in slots))

    def test_unsupported_alignment_fails_at_build(self):
        elf = self.compile('''
            volatile int counter __attribute__((aligned(256))) = 1;
            int gm_plugin_entry(void) { return counter; }
        ''')
        with self.assertRaises(PackageError): pack(elf, MANIFEST)

    def test_symbolic_memory_pseudos_keep_access_after_address_binding(self):
        result = normalize('\tlw a5,counter\n\tflw fa0,value,a4\n\tsw a0,counter,a5\n')
        self.assertIn('lla a5,counter', result)
        self.assertIn('lw a5,0(a5)', result)
        self.assertIn('flw fa0,0(a4)', result)
        self.assertIn('sw a0,0(a5)', result)
        with self.assertRaises(ValueError): normalize('\tsw a0,counter\n')

if __name__ == '__main__': unittest.main()
