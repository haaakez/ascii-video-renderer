#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
output_dir="${1:-$repo_dir/release}"
stage_dir="$(mktemp -d)"

cleanup() {
    rm -rf "$stage_dir"
}
trap cleanup EXIT

mkdir -p "$output_dir"
cd "$repo_dir"

echo "Creating self-contained Linux executable..."
system="$(nix eval --raw --impure --expr builtins.currentSystem)"
release_dir="$stage_dir/ascii-video-linux"
mkdir -p "$release_dir/.runtime-home"

nix bundle ".#packages.${system}.default" --no-write-lock-file \
    -o "$release_dir/.ascii-video-bundle"

# Nix returns a symlink to the bundle in the store.  Run it once during
# packaging so the release contains the expanded runtime and never has to
# unpack hundreds of megabytes on the user's first launch.
bundle_path="$(readlink -f "$release_dir/.ascii-video-bundle")"
HOME="$release_dir/.runtime-home" "$bundle_path" --no-run

# Keep a real copy in the release; the path returned by `nix bundle` is
# normally a symlink into /nix/store and would be broken after extraction.
cp -L "$bundle_path" "$release_dir/.ascii-video-bundle.real"
mv -f "$release_dir/.ascii-video-bundle.real" "$release_dir/.ascii-video-bundle"

cp scripts/run-bundled.sh "$release_dir/ascii-video"
cp scripts/install-desktop.sh "$release_dir/install-desktop.sh"
chmod +x "$release_dir/ascii-video"
chmod +x "$release_dir/install-desktop.sh"
cp README.md "$release_dir/README.md"

mkdir -p "$output_dir"
cp -a "$release_dir" "$output_dir/"
tar -C "$stage_dir" -czf "$output_dir/ascii-video-linux.tar.gz" \
    ascii-video-linux

echo "Wrote: $output_dir/ascii-video-linux/ascii-video"
echo "Wrote: $output_dir/ascii-video-linux.tar.gz"
