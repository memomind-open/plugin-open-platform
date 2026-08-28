#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
studio_root="$(cd "$script_dir/.." && pwd)"
repository_root="$(cd "$studio_root/.." && pwd)"
image="memomind-plugin-studio-linux-x64:ubuntu-24.04"
build_root="$studio_root/.build/linux-x64"
target_root="$build_root/target"

if ! docker info >/dev/null 2>&1; then
  echo "Docker is unavailable to the current user. Log in again after joining the docker group." >&2
  exit 1
fi

for attempt in 1 2 3; do
  if docker build \
    --file "$script_dir/Dockerfile.linux-x64" \
    --tag "$image" \
    "$script_dir"; then
    break
  fi
  if [[ "$attempt" == 3 ]]; then
    echo "Docker image build failed after three attempts." >&2
    exit 1
  fi
  echo "Docker image build failed; retrying ($attempt/3)..." >&2
done

mkdir -p "$target_root"
docker run --rm \
  --env APPIMAGE_EXTRACT_AND_RUN=1 \
  --env CARGO_HOME=/opt/cargo \
  --env CARGO_TARGET_DIR=/workspace/Studio/.build/linux-x64/target \
  --env HOST_GID="$(id -g)" \
  --env HOST_UID="$(id -u)" \
  --env HOME=/root \
  --env PATH=/opt/cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  --env RUSTUP_HOME=/opt/rustup \
  --volume "$repository_root:/workspace" \
  "$image" \
  bash -lc 'cd /workspace/Studio/desktop/src-tauri && cargo tauri build --bundles appimage && chown -R "$HOST_UID:$HOST_GID" /workspace/Studio/.build/linux-x64'

appimage="$(find "$target_root/release/bundle/appimage" -maxdepth 1 -type f -name '*.AppImage' -print -quit)"

if [[ -z "$appimage" ]]; then
  echo "Expected an AppImage bundle, but none was generated." >&2
  exit 1
fi

node "$studio_root/scripts/export-studio.mjs" \
  --platform linux-x64 \
  --single-file \
  --input "$appimage"
