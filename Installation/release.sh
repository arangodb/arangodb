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

fgrep -q "v$VERSION" CHANGELOG

if [ "$?" != 0 ];  then
  echo "$0: version $VERSION not defined in CHANGELOG"
  exit 1
fi

echo "$VERSION" > VERSION

cat configure.ac \
  | sed -e 's~AC_INIT(\[\(.*\)\], \[.*\..*\..*\], \[\(.*\)\], \[\(.*\)\], \[\(.*\)\])~AC_INIT([\1], ['$VERSION'], [\2], [\3], [\4\])~' \
  > configure.ac.tmp

mv configure.ac.tmp configure.ac

./configure --enable-maintainer-mode CPPFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib
make built-sources
make add-maintainer
make add-automagic

make
make examples
make swagger

git add -f Documentation/Examples/*.generated

cd Documentation/Books; make

case "$TAG" in
  *-alpha*|*-beta*|devel)
    git rm -f EXPERIMENTAL
    ;;
esac

if [ "$TAG" == "1" ];  then
  git commit -m "release version $VERSION" -a
  git push

  git tag "v$VERSION"
  git push --tags
fi
