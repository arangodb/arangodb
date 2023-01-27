#!/usr/bin/env bash

echo "running clang-format -i for all files in arangod/, lib/, and client-tools/"

set -xe

HEADERNAMES=`mktemp`

clang-format --version

# clang-format incorrectly deems some .h files in the ArangoDB repository
# to be ObjectiveC files; we sidestep this problem by renaming them all
# to .hpp, then running clang-format, and moving them back to their
# original name.
find client-tools lib arangod tests -iname \*.h > $HEADERNAMES

for file in $(cat $HEADERNAMES) ;
do
  mv $file $(dirname $file)/$(basename $file .h).hpp
done

find arangod lib client-tools tests \
  \( -name '*.cpp' -o -name '*.hpp' -o -name '*.tpp' \) \
  -type f \
  -exec clang-format -i {} \+

for file in $(cat $HEADERNAMES) ;
do
  mv $(dirname $file)/$(basename $file .h).hpp $file
done
