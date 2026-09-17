#!/usr/bin/env python3
"""Public command entry for the unified GMP v2 Flash/RAM package format."""
import struct
import sys
from gmp_xip_pack import main
from gmp_elf import PackageError

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TypeError, struct.error, PackageError) as error:
        print(f'gmp_pack: {error}', file=sys.stderr)
        sys.exit(2)
