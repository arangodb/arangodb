#!/usr/bin/env bash

echo "running clang-format -i for all files in arangod/, lib/, and client-tools/"

set -xe

HEADERNAMES=`mktemp`

clang-format --version

find client-tools lib arangod tests -iname \*.h >> $HEADERNAMES

for file in $(cat $HEADERNAMES) ;
do
  mv $file $(dirname $file)/$(basename $file .h).hpp
done

clang-format -i arangod/**/*.{cpp,hpp}
clang-format -i lib/**/*.{cpp,hpp}
clang-format -i client-tools/**/*.{cpp,hpp}
clang-format -i tests/**/*.{cpp,hpp}

for file in $(cat $HEADERNAMES) ;
do
  mv $(dirname $file)/$(basename $file .h).hpp $file
done
