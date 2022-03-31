#!/bin/bash

# Copyright 2019 - 2020 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt)

set -euo pipefail

cd "$(dirname "$0")/.."

if ! [ -e "tools/create_standalone.sh" ]; then
  echo "Could not change to repo root"
  exit 1
fi

targetFolder="${1:-nowide_standalone}"

# If target folder exists fail, unless it is the default in which case it is removed
if [ -e "$targetFolder" ]; then
  if [[ "$targetFolder" == "nowide_standalone" ]]; then
    rm -r "$targetFolder"
  else
    echo "Target folder $targetFolder exists"
    exit 1
  fi
fi

mkdir -p "$targetFolder"/include

cp -r include/boost/nowide "$targetFolder"/include
cp -r config src test cmake CMakeLists.txt LICENSE README.md "$targetFolder"
cp standalone/*.hpp "$targetFolder"/include/nowide
mv "$targetFolder/cmake/BoostAddWarnings.cmake" "$targetFolder/cmake/NowideAddWarnings.cmake"
find "$targetFolder" -name 'Jamfile*' -delete

SOURCES=$(find "$targetFolder" -name '*.hpp' -or -name '*.cpp')
SOURCES_NO_BOOST=$(echo "$SOURCES" | grep -v 'filesystem.hpp')

sed 's/BOOST_NOWIDE_/NOWIDE_/g' -i $SOURCES
sed 's/BOOST_/NOWIDE_/g' -i $SOURCES
sed 's/boost::nowide/nowide/g' -i $SOURCES
sed 's/boost::/nowide::/g' -i $SOURCES_NO_BOOST
sed '/namespace boost/d' -i $SOURCES
sed 's/<boost\/nowide\//<nowide\//g' -i $SOURCES
sed 's/<boost\//<nowide\//g' -i $SOURCES_NO_BOOST
sed '/config\/abi_/d' -i $SOURCES

CMLs=$(find "$targetFolder" -name 'CMakeLists.txt' -or -name '*.cmake')

sed 's/ BOOST_ALL_NO_LIB//' -i $CMLs
sed 's/BOOST_NOWIDE_/NOWIDE_/g' -i $CMLs
sed 's/Boost_NOWIDE_/NOWIDE_/g' -i $CMLs
sed 's/boost_nowide/nowide/g' -i $CMLs
sed 's/boost_/nowide_/g' -i $CMLs
sed 's/Boost::nowide/nowide::nowide/g' -i $CMLs
sed 's/Boost/Nowide/g' -i $CMLs
sed 's/ OR BOOST_SUPERPROJECT_SOURCE_DIR//' -i $CMLs

sed '/PUBLIC BOOST_NOWIDE_NO_LIB)/d' -i "$targetFolder/CMakeLists.txt"
sed '/^if(BOOST_SUPERPROJECT_SOURCE_DIR/,/^endif/d' -i "$targetFolder/CMakeLists.txt"
sed '/^elseif(BOOST_SUPERPROJECT_SOURCE_DIR)/,/^else/{/^else(/!d}' -i "$targetFolder/CMakeLists.txt"
sed '/^if(NOT BOOST_SUPERPROJECT_SOURCE_DIR)/,/^endif/{/^if/d;/^endif/d}' -i "$targetFolder/CMakeLists.txt"
sed 's/NAMESPACE Nowide CONFIG_FILE.*$/NAMESPACE nowide)/' -i "$targetFolder/CMakeLists.txt"

sed '/^if(NOT BOOST_SUPERPROJECT_SOURCE_DIR)/,/^endif/d' -i "$targetFolder/test/CMakeLists.txt"
sed '/Nowide::filesystem/d' -i "$targetFolder/test/CMakeLists.txt"

sed '/^# Those 2 should work the same/,/^elseif/d' -i "$targetFolder/test/cmake_test/CMakeLists.txt"
sed '/^else/,/^endif/d' -i "$targetFolder/test/cmake_test/CMakeLists.txt"
