{
  description = "CPU GTK4 ASCII video renderer";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          gst = pkgs.gst_all_1;
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "ascii-video";
            version = "0.1.0";
            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              pkg-config
              wrapGAppsHook4
            ];

            buildInputs = [
              pkgs.gtk4
              pkgs.cairo
              gst.gstreamer
              gst.gst-plugins-base
              gst.gst-plugins-good
              gst.gst-plugins-bad
              gst.gst-plugins-ugly
              gst.gst-libav
              pkgs.gsettings-desktop-schemas
              pkgs.libepoxy
              pkgs.ffmpeg
              pkgs.fontconfig
            ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DASCII_VIDEO_FFMPEG_PATH=${pkgs.ffmpeg}/bin/ffmpeg"
            ];

            # Do not let a user's GTK3 theme environment override the GTK4
            # runtime bundled with this package.  In particular, GTK_PATH can
            # make GTK4 try to load a GTK3-only Adwaita-dark theme from the
            # user's profile.
            preFixup = ''
              gappsWrapperArgs+=(--unset GTK_THEME --unset GTK_PATH)
              gappsWrapperArgs+=(--set FONTCONFIG_FILE "${pkgs.fontconfig.out}/etc/fonts/fonts.conf")
              gappsWrapperArgs+=(--set-default LANG C.UTF-8)
            '';

            meta.mainProgram = "ascii-video";
          };
        });

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          gst = pkgs.gst_all_1;
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              cmake
              pkg-config
              gcc
              gdb
              gst.gstreamer
              gst.gst-plugins-base
              gst.gst-plugins-good
              gst.gst-plugins-bad
              gst.gst-plugins-ugly
              gst.gst-libav
              gsettings-desktop-schemas
            ];

            buildInputs = [
              pkgs.gtk4
              pkgs.cairo
              pkgs.gsettings-desktop-schemas
              pkgs.libepoxy
              pkgs.ffmpeg
            ];

            shellHook = ''
              echo "ascii-video development shell"
              echo "Build with: cmake -S . -B build && cmake --build build -j"
              export GSETTINGS_SCHEMA_DIR="${pkgs.gtk4}/share/gsettings-schemas/gtk4-${pkgs.gtk4.version}/glib-2.0/schemas"
            '';
          };
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/ascii-video";
        };
      });
    };
}
