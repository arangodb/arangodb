#!/bin/bash
rm -f cppcheck.log cppcheck.tmp
touch cppcheck.tmp

cppcheck -j4 \
  --std=c++11 \
  --enable=style,warning \
  --force \
  --quiet \
  --platform=unix64 \
  --inline-suppr \
  --suppress="*:lib/JsonParser/json-parser.cpp" \
  --suppress="*:lib/V8/v8-json.cpp" \
  --suppress="*:lib/Zip/crypt.h" \
  --suppress="*:lib/Zip/iowin32.cpp" \
  --suppress="*:lib/Zip/unzip.cpp" \
  --suppress="*:lib/Zip/zip.cpp" \
  --suppress="*:Aql/grammar.cpp" \
  --suppress="*:Aql/tokens.cpp" \
  --suppress="*:Aql/tokens.ll" \
  arangod/ arangosh/ lib/ 2>> cppcheck.tmp

sort cppcheck.tmp | uniq > cppcheck.log
rm cppcheck.tmp
cat cppcheck.log
