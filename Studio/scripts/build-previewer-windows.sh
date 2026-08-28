#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
studio_root="$(cd "$script_dir/.." && pwd)"
source_root="$studio_root/previewer"
build_root="$studio_root/.build/previewer/windows-mingw-x64"

cmake -S "$source_root" -B "$build_root" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
cmake --build "$build_root" --parallel
