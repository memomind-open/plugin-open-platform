#!/usr/bin/env python3
"""CMake compiler launcher: materialize symbolic addresses before loads/stores.

RISC-V assembler's `lw rd,symbol` combines an address pair and a memory access.
XIP binds the address pair to a RAM address slot, so preserve that distinction.
Only compiler-emitted symbolic pseudo instructions are expanded. Ordinary
register-indirect operations stay unchanged; the packer validates relocations.
"""
from __future__ import annotations
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

LOADS = {'lb', 'lbu', 'lh', 'lhu', 'lw', 'lwu', 'ld', 'flw', 'fld'}
STORES = {'sb', 'sh', 'sw', 'sd', 'fsw', 'fsd'}
REG = re.compile(r'^(?:x(?:[0-9]|[12][0-9]|3[01])|zero|ra|sp|gp|tp|t[0-6]|s(?:[0-9]|1[01])|a[0-7])$')
MAX_FUNCTION_STACK = 1024
STACK_HINT = ('Move large local buffers to host->alloc()/host->free() '
              '(the SDK malloc/free equivalent), check NULL, and release ownership correctly. '
              'This is a per-function limit, not a whole-call-chain guarantee.')


def check_stack_usage(report: pathlib.Path) -> int:
    """Check compiler-emitted frames, including inlining and saved registers."""
    if not report.is_file():
        raise ValueError('missing GCC stack-usage report; stack usage cannot be verified')
    maximum = 0
    for line in report.read_text().splitlines():
        fields = line.rsplit('\t', 2)
        if len(fields) != 3 or not fields[1].isdigit():
            raise ValueError('invalid GCC stack-usage record: ' + line)
        function, count, kind = fields
        size = int(count)
        if kind != 'static':
            raise ValueError(f'{function}: dynamic/unknown stack usage ({kind}) is forbidden. {STACK_HINT}')
        if size > MAX_FUNCTION_STACK:
            raise ValueError(f'{function}: stack frame {size} bytes exceeds '
                             f'{MAX_FUNCTION_STACK} bytes. {STACK_HINT}')
        maximum = max(maximum, size)
    return maximum

def normalize(assembly: str) -> str:
    output = []
    for line in assembly.splitlines():
        match = re.fullmatch(r'\s*([a-z]+)\s+([^#]+?)(?:\s*#.*)?', line)
        if match:
            op, operands = match.groups()
            args = [v.strip() for v in operands.split(',')]
            if op in LOADS | STORES and len(args) in (2, 3):
                symbol = args[1]
                if '(' not in symbol and re.match(r'^[.$A-Za-z_]', symbol) and not REG.fullmatch(symbol):
                    scratch = args[2] if len(args) == 3 else args[0]
                    if not REG.fullmatch(scratch) or scratch in ('zero', 'x0') or (op in STORES and len(args) != 3):
                        raise ValueError('unsupported symbolic memory operand: ' + line)
                    output.extend(['\t.option push', '\t.option norelax', '\t.option norvc',
                                   f'\tlla {scratch},{symbol}', '\t.option pop',
                                   f'\t{op} {args[0]},0({scratch})'])
                    continue
        output.append(line)
    return '\n'.join(output) + '\n'


def main() -> int:
    command = sys.argv[1:]
    if '-c' not in command or '-o' not in command:
        return subprocess.call(command)
    source_index = command.index('-c') + 1
    output_index = command.index('-o') + 1
    source = pathlib.Path(command[source_index])
    if source.suffix != '.c':
        return subprocess.call(command)
    output = pathlib.Path(command[output_index])
    saved_report = output.with_suffix('.su')
    # A failed incremental build must not leave a previous frame report/object
    # looking like a freshly checked result.
    saved_report.unlink(missing_ok=True)
    output.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='gm-xip-') as temp:
        assembly = pathlib.Path(temp) / 'plugin.s'
        compile_command = command.copy()
        compile_command[source_index - 1] = '-S'
        compile_command[output_index] = str(assembly)
        compile_command.append('-fstack-usage')
        result = subprocess.call(compile_command)
        if result:
            return result
        report = assembly.with_suffix('.su')
        try:
            maximum = check_stack_usage(report)
        except (OSError, ValueError) as error:
            print(f'gm-plugin stack check: {error}', file=sys.stderr)
            return 2
        assembly.write_text(normalize(assembly.read_text()))
        assemble = command.copy()
        assemble[source_index] = str(assembly)
        # The first invocation already emitted dependency information for C.
        cleaned = []
        skip = False
        for arg in assemble:
            if skip:
                skip = False
            elif arg in ('-MF', '-MT', '-MQ'):
                skip = True
            elif arg not in ('-MD', '-MMD', '-MP'):
                cleaned.append(arg)
        result = subprocess.call(cleaned)
        if result == 0:
            shutil.copyfile(report, saved_report)
            print(f'plugin stack: maximum function frame {maximum}/{MAX_FUNCTION_STACK} B; '
                  f'report={saved_report}')
        return result

if __name__ == '__main__':
    sys.exit(main())
