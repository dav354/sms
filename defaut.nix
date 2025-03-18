{
  description = "Dev shell for ESP-IDF with additional pip dependencies";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    espDev = {
      url = "github:mirrexagon/nixpkgs-esp-dev";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    nixpkgs,
    espDev,
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {inherit system;};
  in {
    devShells.default = pkgs.mkShell {
      buildInputs = [
        espDev.packages.${system}.esp-idf-full
        pkgs.python3
        pkgs.python3Packages.virtualenv
        pkgs.ffmpeg
      ];
      shellHook = ''
        # Create and activate a virtual environment if not already present.
        if [ ! -d .venv ]; then
          echo "Creating virtual environment..."
          python3 -m venv .venv
        fi
        echo "Activating virtual environment..."
        source .venv/bin/activate

        # Upgrade pip and install required Python packages
        pip install --upgrade pip
        pip install aubio pydub
      '';
    };
  };
}
