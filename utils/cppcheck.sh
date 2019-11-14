#!/usr/bin/env sh
rm -f cppcheck.xml cppcheck.xml.tmp

cppcheck $* \
  --xml --xml-version=2 \
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
  --suppress="*:lib/Geo/karney/*" \
  --suppress="*:lib/V8/v8-json.ll" \
  --suppress="*:lib/Zip/*" \
  --suppress="constStatement" \
  --suppress="duplicateCondition" \
  --suppress="duplicateConditionalAssign" \
  --suppress="internalAstError" \
  --suppress="mismatchingContainerExpression" \
  --suppress="missingInclude" \
  --suppress="noExplicitConstructor:lib/Futures/Future.h" \
  --suppress="passedByValue" \
  --suppress="redundantAssignInSwitch" \
  --suppress="redundantAssignment" \
  --suppress="shadowFunction" \
  --suppress="shadowVar" \
  --suppress="stlFindInsert" \
  --suppress="syntaxError" \
  --suppress="uninitMemberVar" \
  --suppress="unreadVariable" \
  --suppress="useStlAlgorithm" \
  --suppress="variableScope" \
  arangod/ arangosh/ lib/ enterprise/ 2> cppcheck.xml
status=$?

cat cppcheck.xml \
  | egrep "<error |<location|</error>" cppcheck.xml \
  | sed -e 's:^.*msg="\([^"]*\)".*:\1:' -e 's:^.*file="\([^"]*\)".*line="\([^"]*\)".*:    \1\:\2:' -e 's:&apos;:":g' -e 's:&gt;:>:g' -e 's:&lt;:<:g' -e 's:</error>::'

cat cppcheck.xml \
  | sed -e "s:file=\":file=\"`pwd`/:g" \
  > cppcheck.xml.tmp
mv cppcheck.xml.tmp cppcheck.xml

exit $status
