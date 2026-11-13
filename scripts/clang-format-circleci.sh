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
find $PATHS -iname \*.h \
  \! -wholename "lib/Basics/sdt.h" \
  \! -wholename "lib/iresearch.build/external/snowball/libstemmer/modules.h" \
  -type f \
  -exec clang-format --assume-filename foo.hpp -i {} \+

# excluding sdt.h fixes the clang-format job; sdt is
# imported as a 3rd party include as far as I can tell
# and maybe should be moved otherplace
find $PATHS \
  \( -name '*.cpp' -o -name '*.tpp' \) \
  -type f \
  -exec clang-format -i {} \+

