#!/usr/bin/env bash
rm -f cppcheck.log cppcheck.tmp
trap "rm -rf cppcheck.tmp" EXIT
touch cppcheck.tmp

cppcheck $* \
  -I lib \
  -I enterprise \
  -I arangod \
  -I arangosh \
  --std=c++17 \
  --enable=warning,style,performance,portability,missingInclude \
  --force \
  --quiet \
  --platform=unix64 \
  --inline-suppr \
  --suppress="*:*yacc.c*" \
  --suppress="*:Aql/grammar.cpp" \
  --suppress="*:Aql/tokens.cpp" \
  --suppress="*:Aql/tokens.ll" \
  --suppress="*:lib/Basics/Endian.h" \
  --suppress="*:lib/Basics/fpconv.cpp" \
  --suppress="*:lib/Basics/memory-map-win32.cpp" \
  --suppress="*:lib/Basics/short_alloc.h" \
  --suppress="*:lib/Basics/xxhash.cpp" \
  --suppress="*:lib/Futures/function2/function2.hpp" \
  --suppress="*:lib/V8/v8-json.ll" \
  --suppress="*:lib/Zip/*" \
  --suppress="duplicateCondition" \
  --suppress="duplicateConditionalAssign" \
  --suppress="noExplicitConstructor:lib/Futures/Future.h" \
  --suppress="passedByValue" \
  --suppress="redundantAssignInSwitch" \
  --suppress="redundantAssignment" \
  --suppress="shadowFunction" \
  --suppress="shadowVar" \
  --suppress="uninitMemberVar" \
  --suppress="unreadVariable" \
  --suppress="useStlAlgorithm" \
  --suppress="variableScope" \
  lib/ 2>> cppcheck.tmp
#  arangod/ arangosh/ lib/ enterprise/ 2>> cppcheck.tmp

sort cppcheck.tmp | uniq > cppcheck.log
cat cppcheck.log
