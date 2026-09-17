"""Actual RV32 compiler checks; oversized buffers cannot be optimized away."""
import pathlib
import os
import subprocess
import sys
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / 'build-host/tools'
sys.path.insert(0, str(TOOLS))
from xip_compile import check_stack_usage

CC = os.environ.get('GM_XIP_TEST_CC', '/opt/riscv-toolchain/10.2.0/bin/riscv64-unknown-elf-gcc')


class StackUsageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='gm-stack-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.source = self.root / 'plugin.c'
        self.object = self.root / 'plugin.c.o'
        self.report = self.root / 'plugin.c.su'

    def compile(self, source):
        self.source.write_text(source)
        return subprocess.run([sys.executable, str(TOOLS / 'xip_compile.py'), CC,
            '-march=rv32imac', '-mabi=ilp32', '-Os', '-fPIC',
            '-c', str(self.source), '-o', str(self.object)], capture_output=True, text=True)

    def test_exact_function_frame_limit_and_saved_report(self):
        result = self.compile('int f(int n) { volatile char b[1024]; b[n&1023]=1; return b[0]; }')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(check_stack_usage(self.report), 1024)
        self.assertTrue(self.object.is_file())

    def test_oversized_incremental_build_removes_old_object_and_report(self):
        self.assertEqual(self.compile('int f(void) { return 3; }').returncode, 0)
        result = self.compile('int f(int n) { volatile char b[1040]; b[n&1023]=1; return b[0]; }')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('exceeds 1024 bytes', result.stderr)
        self.assertIn('host->alloc()', result.stderr)
        self.assertFalse(self.object.exists())
        self.assertFalse(self.report.exists())

    def test_dynamic_vla_and_alloca_are_rejected(self):
        for body in [
            'volatile char b[n]; b[n-1]=1; return b[0];',
            'volatile char *b=__builtin_alloca(n); b[n-1]=1; return b[0];',
        ]:
            with self.subTest(body=body):
                result = self.compile('int f(int n) {' + body + '}')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('dynamic/unknown stack', result.stderr)
                self.assertFalse(self.object.exists())

    def test_translation_unit_without_functions_is_valid(self):
        result = self.compile('extern int function(void);')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(check_stack_usage(self.report), 0)

    def test_missing_or_malformed_reports_fail_closed(self):
        with self.assertRaisesRegex(ValueError, 'missing'):
            check_stack_usage(self.report)
        self.report.write_text('not a GCC stack record\n')
        with self.assertRaisesRegex(ValueError, 'invalid'):
            check_stack_usage(self.report)


if __name__ == '__main__':
    unittest.main()
