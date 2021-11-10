#!/bin/bash

# Switch to directory of this script
MYDIR=$(dirname $(realpath "$0"))
cd "${MYDIR}"

# Exit if anything fails
set -e

mkdir -p build
cd build
cmake .. -DHWY_WARNINGS_ARE_ERRORS:BOOL=ON
make -j
ctest -j
echo Success
