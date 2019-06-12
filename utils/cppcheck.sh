#!/usr/bin/env bash
rm -f cppcheck.log cppcheck.tmp
trap "rm -rf cppcheck.tmp" EXIT
touch cppcheck.tmp

cppcheck -j4 \
  --std=c++11 \
  --enable=style,warning \
  --force \
  --quiet \
  --platform=unix64 \
  --inline-suppr \
  --suppress="*:lib/V8/v8-json.cpp" \
  --suppress="*:lib/Zip/crypt.h" \
  --suppress="*:lib/Zip/iowin32.cpp" \
  --suppress="*:lib/Zip/unzip.cpp" \
  --suppress="*:lib/Zip/zip.cpp" \
  --suppress="*:Aql/grammar.cpp" \
  --suppress="*:Aql/tokens.cpp" \
  --suppress="*:Aql/tokens.ll" \
  arangod/ arangosh/ lib/ enterprise/ 2>> cppcheck.tmp

sort cppcheck.tmp | uniq > cppcheck.log
cat cppcheck.log
