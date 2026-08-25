#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sdk_build="${GMPLUGIN_SDK_BUILD:-$project_dir/../../GlassSDK/build-host}"
cli="$project_dir/build/gmplugin-preview-cli"

if [[ ! -x "$cli" ]]; then
  "$project_dir/build.sh"
fi
if [[ ! -d "$sdk_build" ]]; then
  echo "SDK example build directory not found: $sdk_build" >&2
  exit 2
fi

temporary_dir="$(mktemp -d)"
trap 'rm -rf "$temporary_dir"' EXIT

count=0
while IFS= read -r -d '' plugin; do
  output="$temporary_dir/$(basename "$plugin").log"
  "$cli" "$plugin" --frames 5 >"$output" 2>&1
  if ! rg -q '\[Previewer\] OK' "$output"; then
    cat "$output" >&2
    exit 1
  fi
  printf 'PASS %s\n' "${plugin#"$sdk_build"/}"
  count=$((count + 1))
done < <(find "$sdk_build" -type f -name '*.gmp' -print0 | sort -z)

"$cli" "$sdk_build/bluetooth/bluetooth.gmp" --bt 1 "smoke test" \
  >"$temporary_dir/bluetooth-event.log" 2>&1
rg -q '\[Bluetooth out\] service=0x0f command=0x29 channel=1' \
  "$temporary_dir/bluetooth-event.log"

"$cli" "$sdk_build/input/input.gmp" --button 1 \
  >"$temporary_dir/button-event.log" 2>&1
rg -q 'button=1 action=1' "$temporary_dir/button-event.log"

"$cli" "$sdk_build/imu/imu.gmp" --gesture 1 \
  >"$temporary_dir/imu-event.log" 2>&1
rg -q 'imu gesture=1 active=1' "$temporary_dir/imu-event.log"

"$cli" "$sdk_build/framebuffer/framebuffer.gmp" --frames 1 \
  --pgm "$temporary_dir/framebuffer.pgm" >"$temporary_dir/framebuffer.log" 2>&1
test -s "$temporary_dir/framebuffer.pgm"
rg -q 'checker stripes displayed' "$temporary_dir/framebuffer.log"

printf 'PASS events and framebuffer\n'
printf 'All %d GMP examples passed.\n' "$count"
