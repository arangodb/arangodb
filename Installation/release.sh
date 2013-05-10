#!/bin/bash

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

git tag | grep -q "^v$VERSION\$"

if [ "$?" == 0 ];  then
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
  > configure.ac.tmp \
 || exit 1

mv configure.ac.tmp configure.ac

./configure --enable-all-in-one-v8 --enable-all-in-one-libev --enable-all-in-one-icu --enable-maintainer-mode --disable-mruby || exit 1
make built-sources || exit 1
make add-maintainer || exit 1
make add-automagic || exit 1
make doxygen || exit 1
make latex || exit 1
make wiki || exit 1

if [ "$TAG" == "1" ];  then
  git commit -m "release version $VERSION" -a
  git push

  git tag "v$VERSION"
  git push --tags
fi
