#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
studio_root="$(cd "$script_dir/.." && pwd)"
source_root="$studio_root/previewer"
build_root="$studio_root/.build/previewer/linux-release"

cmake -S "$source_root" -B "$build_root" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_root" --parallel
