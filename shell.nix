{pkgs ? import <nixpkgs> {}}: let
  libps2000Repo = pkgs.fetchgit {
    url = "https://github.com/ginloy/libps2000";
    hash = "sha256-gwNLIqXwYtfmj6nyaOJmHAsxBNK5XUcZNH3EVW0E9tk=";
  };
  libps2000 = pkgs.callPackage libps2000Repo {};
  # stdenv = pkgs.llvmPackages.libcxxStdenv;
in
  (pkgs.mkShell.override { stdenv = pkgs.clangStdenv; }) {
  # pkgs.mkShell {
    nativeBuildInputs = with pkgs; [
      meson
      ninja
      cmake
      pkg-config
      linuxPackages_latest.perf
      clang-tools
      cmake-language-server
      cmake-format
    ];

    buildInputs = with pkgs; [
      fftw
      imgui
      implot
      libps2000
      gtest
      range-v3
      llvmPackages.openmp
    ];
  }
