# ASCII video renderer

ASCII video renderer is a GTK4 application that converts video frames to ASCII art. It
uses GStreamer for playback and a multithreaded CPU renderer for previews and
exports. It does not require OpenGL or Vulkan and uses the system GTK theme.

![screenshot](image.png)

## functions

- displays the original video and the ASCII result
- supports seeking, playback speed, and looping
- supports custom ramps, colors, palettes, and tone controls
- includes presets, thresholding, inversion, and saturation controls
- exports MP4, MOV, animated PNG, and GIF files
- uses lossless mode by default for MP4 and MOV
- renders high-resolution output with multiple CPU threads
- supports native, preview, half-native, and custom output resolutions

## dependencies

the Nix flake provides the required development dependencies:

- GTK4
- Cairo
- GStreamer with app, video, libav, and x264 support
- libepoxy
- FFmpeg
- CMake and a C11 compiler

## build and run with Nix

```sh
nix develop --no-write-lock-file path:.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ascii-video 
```

to run the application through the flake:

```sh
nix run . -- 
```

to create a standalone Linux executable and a tar archive, use:

```sh
./scripts/package.sh
./release/ascii-video-linux/ascii-video
```


## build without Nix

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ascii-video 
```
