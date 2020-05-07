{ compiler ? "",
  nixpkgs ? (import <nixpkgs> {}).fetchFromGitHub {
    owner  = "NixOS";
    repo   = "nixpkgs";
    rev    = "adaa9d93488c7fc1fee020899cb08ce2f899c94b";
    sha256 = "0rkfxgmdammpq4xbsx7994bljqhzjjblpky2pqmkgn6x9s6adwrr";
  }}:

with import nixpkgs {};

let
  # For the documentation tools we use an older Nixpkgs since the
  # newer versions seem to be not working great...
  oldNixpkgsSrc = fetchFromGitHub {
                    owner  = "NixOS";
                    repo   = "nixpkgs";
                    rev    = "d0d905668c010b65795b57afdf7f0360aac6245b";
                    sha256 = "1kqxfmsik1s1jsmim20n5l4kq6wq8743h5h17igfxxbbwwqry88l";
                  };
  oldNixpkgs    = import oldNixpkgsSrc {};
  docs          = import ./nix/docs.nix { nixpkgs = oldNixpkgsSrc; };
  benchmarks    = import ./nix/benchmarks.nix { inherit nixpkgs; };
  compilerPkg   = if compiler != ""
                  then pkgs.${compiler}
                  else stdenv.cc;
  theStdenv     = if compilerPkg.isClang
                  then clangStdenv
                  else stdenv;
in

theStdenv.mkDerivation rec {
  name = "immer-env";
  buildInputs = [
    compilerPkg
    git
    cmake
    pkgconfig
    ninja
    gdb
    lldb
    ccache
    boost
    boehmgc
    benchmarks.c_rrb
    benchmarks.steady
    benchmarks.chunkedseq
    benchmarks.immutable_cpp
    benchmarks.hash_trie
    oldNixpkgs.doxygen
    (oldNixpkgs.python.withPackages (ps: [
      ps.sphinx
      docs.breathe
      docs.recommonmark
    ]))
  ];
  shellHook = ''
    export CC=${compilerPkg}/bin/cc
    export CXX=${compilerPkg}/bin/c++
  '';
}
