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

# excluding sdt.h fixes the clang-format job; sdt is
# imported as a 3rd party include as far as I can tell
# and maybe should be moved otherplace
find arangod lib client-tools tests \
  \( -name '*.cpp' -o -name '*.hpp' -o -name '*.tpp' \) \
  \! -wholename lib/Basics/sdt.hpp \
  -type f \
  -exec clang-format -i {} \+

for file in $(cat $HEADERNAMES) ;
do
  mv $(dirname $file)/$(basename $file .h).hpp $file
done
