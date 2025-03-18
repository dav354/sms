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
    self,
    nixpkgs,
    espDev,
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {inherit system;};
  in {
    devShells.${system}.default = pkgs.mkShell {
      buildInputs =[
        espDev.packages.${system}.esp-idf-full
        pkgs.python313
        pkgs.python3Packages.virtualenv
        pkgs.ffmpeg
        pkgs.python313Packages.aubio
        pkgs.python313Packages.pydub
      ];
    };
  };
}
