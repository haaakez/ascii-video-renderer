#!/bin/sh
set -eu

app_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
user_home="${HOME:-$(pwd)}"

# Keep user-facing configuration and data in the user's home directory while
# reserving HOME inside the release for the pre-extracted Nix runtime cache.
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$user_home/.config}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-$user_home/.local/share}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$user_home/.cache}"
export HOME="$app_dir/.runtime-home"

if [ "${ASCII_VIDEO_FOREGROUND:-0}" = 1 ]; then
    exec "$app_dir/.ascii-video-bundle" "$@"
fi

# The release is a GUI application.  Detach it from a terminal when launched
# directly, while retaining a foreground mode for troubleshooting.
setsid "$app_dir/.ascii-video-bundle" "$@" \
    </dev/null >/dev/null 2>&1 &
