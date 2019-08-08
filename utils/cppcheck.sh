#!/usr/bin/env sh
rm -f cppcheck.log cppcheck.tmp
trap "rm -rf cppcheck.tmp" EXIT
touch cppcheck.tmp

cppcheck $* \
  -I arangod \
  -I arangosh \
  -I build/arangod \
  -I build/arangosh \
  -I build/lib \
  -I enterprise \
  -I lib \
  -D USE_PLAN_CACHE \
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
  --suppress="mismatchingContainerExpression" \
  --suppress="noExplicitConstructor:lib/Futures/Future.h" \
  --suppress="passedByValue" \
  --suppress="redundantAssignInSwitch" \
  --suppress="redundantAssignment" \
  --suppress="shadowFunction" \
  --suppress="shadowVar" \
  --suppress="stlFindInsert" \
  --suppress="uninitMemberVar" \
  --suppress="unreadVariable" \
  --suppress="useStlAlgorithm" \
  --suppress="variableScope" \
  arangod/ arangosh/ lib/ enterprise/ 2>> cppcheck.tmp

cat cppcheck.tmp \
  | fgrep -v "Syntax Error: AST broken, binary operator '=' doesn't have two operands" \
  | fgrep -v "Found suspicious operator ','" \
  | fgrep -v "SymbolDatabase bailout; unhandled code" \
  | fgrep -v "Cppcheck cannot find all the include files" \
  | sort \
  | uniq > cppcheck.log
cat cppcheck.log

test ! -s cppcheck.log
