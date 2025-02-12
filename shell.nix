{stable ? import <nixpkgs> {}, pkgs? import <unstable> {}}: let
  libps2000Repo = builtins.fetchGit {
    url = "https://github.com/ginloy/libps2000";
  };
  libps2000 = stable.callPackage libps2000Repo {};
in
  pkgs.mkShell {
    nativeBuildInputs = with pkgs; [
      meson
      ninja
      cmake
      pkg-config
      linuxPackages_latest.perf
      clang-tools
    ];

    buildInputs = with pkgs; [
      fftw
      imgui
      implot
      libps2000
      gtest
      range-v3
    ];
  }
