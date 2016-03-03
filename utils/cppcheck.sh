#!/bin/bash
rm -f cppcheck.log cppcheck.tmp
touch cppcheck.tmp

for platform in unix32 unix64; do
  cppcheck -j4 \
    --std=c++11 \
    --enable=style \
    --force \
    --platform=$platform \
    --suppress="*:lib/JsonParser/json-parser.cpp" \
    --suppress="*:lib/V8/v8-json.cpp" \
    --suppress="*:arangod/Aql/grammar.cpp" \
    --suppress="*:arangod/Aql/tokens.cpp" \
    arangod/ arangosh/ lib/ 2>> cppcheck.tmp
done

sort cppcheck.tmp | uniq > cppcheck.log
rm cppcheck.tmp
cat cppcheck.log
