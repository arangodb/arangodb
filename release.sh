#!/bin/bash

if [ "$#" -ne 1 ];  then
  echo "usage: $0 <major>.<minor>.<patchlevel>"
  exit 1
fi

VERSION="$1"

git tag | fgrep -q "v$VERSION"

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

./configure --enable-all-in-one --disable-flex --disable-bison --disable-mruby
make built-sources
make doxygen
make latex
make wiki

git commit -m "release version $VERSION" -a
git push

git tag "v$VERSION"
git push --tags
