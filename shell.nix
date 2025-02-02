{pkgs ? import <nixpkgs> {}}: let
  libps2000Repo = builtins.fetchGit {
    url = "https://github.com/ginloy/libps2000";
  };
  libps2000 = pkgs.callPackage libps2000Repo {};
in
  pkgs.mkShell {
    nativeBuildInputs = with pkgs; [
      meson
      ninja
      cmake
      pkg-config
    ];

    buildInputs = with pkgs; [
      fftw
      imgui
      implot
      libps2000
    ];
  }
