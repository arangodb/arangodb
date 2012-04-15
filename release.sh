#!/bin/bash

if [ "$#" -ne 1 ];  then
  echo "usage: $0 <major>.<minor>.<pathlevel>"
  exit 1
fi

VERSION="$1"

git tag | fgrep -q "v$VERSION"

if [ "$?" == 0 ];  then
  echo "$0: version $VERSION already defined"
  exit 1
fi

echo "$VERSION" > VERSION

cat configure.ac \
  | sed -e 's~AC_INIT(\[\(.*\)\], \[.*\..*\..*\], \[\(.*\)\], \[\(.*\)\], \[\(.*\)\])~AC_INIT([\1], ['$VERSION'], [\2], [\3], [\4\])~' \
  > configure.ac.tmp \
 || exit 1

mv configure.ac.tmp configure.ac

make configure

git tag "v$VERSION"
git push --tags
