#!/usr/bin/env bash

PATHS="client-tools lib arangod tests enterprise/Enterprise enterprise/tests"

echo "running clang-format -i for all files in $PATHS"

set -e

HEADERNAMES=$(mktemp)
RANDOM_PREFIX=$(cat /dev/urandom | tr -dc '[:alpha:]' | fold -w ${1:-8} | head -n 1)

clang-format --version

# clang-format incorrectly deems some .h files in the ArangoDB repository
# to be ObjectiveC files; we sidestep this problem by renaming them all
# to .hpp, then running clang-format, and moving them back to their
# original name.
find $PATHS -iname \*.h > $HEADERNAMES

for file in $(cat $HEADERNAMES) ;
do
  mv $file $(dirname $file)/"$RANDOM_PREFIX"_"$(basename $file .h)".hpp
done

# excluding sdt.h fixes the clang-format job; sdt is
# imported as a 3rd party include as far as I can tell
# and maybe should be moved otherplace
find $PATHS \
  \( -name '*.cpp' -o -name '*.hpp' -o -name '*.tpp' \) \
  \! -wholename "lib/Basics/${RANDOM_PREFIX}_sdt.hpp" \
  -type f \
  -exec clang-format -i {} \+

for file in $(cat $HEADERNAMES) ;
do
  mv "$(dirname $file)"/"${RANDOM_PREFIX}"_"$(basename $file .h)".hpp $file
done
