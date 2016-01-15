#!/bin/bash
set -e

TAG=1

if [ "$1" == "--no-tag" ];  then
  TAG=0
  shift
fi

if [ "$#" -ne 1 ];  then
  echo "usage: $0 <major>.<minor>.<patchlevel>"
  exit 1
fi

VERSION="$1"

if git tag | grep -q "^v$VERSION$";  then
  echo "$0: version $VERSION already defined"
  exit 1
fi

if fgrep -q "v$VERSION" CHANGELOG;  then
 echo "version $VERSION defined in CHANGELOG" 
else
  echo "$0: version $VERSION not defined in CHANGELOG"
  exit 1
fi

echo "$VERSION" > VERSION

cat configure.ac \
  | sed -e 's~AC_INIT(\[\(.*\)\], \[.*\..*\..*\], \[\(.*\)\], \[\(.*\)\], \[\(.*\)\])~AC_INIT([\1], ['$VERSION'], [\2], [\3], [\4\])~' \
  > configure.ac.tmp

mv configure.ac.tmp configure.ac

if [ `uname` == "Darwin" ];  then
  ./configure \
    --enable-maintainer-mode \
    CPPFLAGS="-I/usr/local/include -I/usr/local/opt/openssl/include" \
    LDFLAGS="-L/usr/local/opt/openssl/lib -L/usr/local/Cellar/boost/1.58.0/lib"
else
  ./configure --enable-maintainer-mode
fi

make built-sources
make add-maintainer
make add-automagic

make
make examples
make swagger

(
  cd js/apps/system/_admin/aardvark/APP
  npm install --only=dev
  npm run grunt
)

git add -f Documentation/Examples/*.generated

cd Documentation/Books; make

case "$TAG" in
  *-alpha*|*-beta*|devel)
    ;;

  *)
    if test -f EXPERIMENTAL; then git rm -f EXPERIMENTAL; fi
    ;;
esac

if [ "$TAG" == "1" ];  then
  git commit -m "release version $VERSION" -a
  git push

  git tag "v$VERSION"
  git push --tags
fi
