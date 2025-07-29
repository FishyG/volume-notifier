{
    description = "A program that notifies when the volume changes";

    inputs = {
        nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    };

    outputs = { self, nixpkgs }: 
        let
            system = "x86_64-linux";
            pkgs = import nixpkgs { inherit system; };
        in
            {
            devShells.${system}.default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } rec {
                buildInputs = with pkgs; [
                    cmake
                    pkg-config
                    libnotify
                    pipewire
                ];
                shellHook = ''
                    if [ ! -d build ]; then
                        mkdir build
                        cd build
                        cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
                        cd ..
                        ln -s build/compile_commands.json ./compile_commands.json
                    fi
                '';
            };

            packages.${system}.default = pkgs.clangStdenv.mkDerivation {
                pname = "volume-notifier";
                version = "0.1";
                nativeBuildInputs = with pkgs; [
                    cmake
                    pkg-config
                    libnotify
                    pipewire
                ];
                src = ./.;
                buildPhase = ''
                    cmake .
                    make
                '';
                installPhase = ''
                    mkdir -p $out/bin
                    cp volume-notifier $out/bin
                '';
            };
    };
}
