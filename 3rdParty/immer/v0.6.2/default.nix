with import <nixpkgs> {};

stdenv.mkDerivation rec {
  name = "immer-git";
  version = "git";
  src = fetchGit ./.;
  nativeBuildInputs = [ cmake ];
  dontBuild = true;
  meta = with stdenv.lib; {
    homepage    = "https://github.com/arximboldi/immer";
    description = "Postmodern immutable data structures for C++";
    license     = licenses.boost;
  };
}
