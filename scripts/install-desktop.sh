#!/bin/sh
set -eu

app_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
install_dir="$data_home/ascii-video"
desktop_dir="$data_home/applications"

mkdir -p "$install_dir" "$desktop_dir"
cp -a "$app_dir"/. "$install_dir"/

desktop_file="$desktop_dir/ascii-video.desktop"
cat > "$desktop_file" <<EOF
[Desktop Entry]
Type=Application
Name=ASCII Video
Comment=Convert video frames to ASCII art
Exec=$install_dir/ascii-video %U
Terminal=false
Categories=AudioVideo;Video;
StartupNotify=true
EOF

chmod +x "$install_dir/ascii-video" "$desktop_file"
printf 'Installed ASCII Video. Launch it from your application menu.\n'
