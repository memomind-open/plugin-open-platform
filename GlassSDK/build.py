#!/usr/bin/env python3
"""Build or inspect GM glasses plugins."""

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(
    0,
    str(Path(__file__).resolve().parent / "build-host" / "tools"),
)

from gm_build import cli


if __name__ == "__main__":
    raise SystemExit(cli())
