#!/usr/bin/env bash

PATHS="client-tools lib arangod tests enterprise/Enterprise enterprise/tests"

echo "running clang-format -i for all files in $PATHS"

set -e

clang-format --version

# sdt.h and modules.h are third-party code
find $PATHS \
  \( -name '*.cpp' -o -name '*.hpp' -o -name '*.tpp' -o -name '*.h' \) \
  \! -wholename "lib/Basics/sdt.h" \
  \! -wholename "lib/iresearch.build/external/snowball/libstemmer/modules.h" \
  -type f \
  -exec clang-format -i {} \+
