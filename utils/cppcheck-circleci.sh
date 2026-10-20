#!/usr/bin/env bash

ferr() { echo "$*"; exit 1; }

if [[ $1 == "-j" ]]; then
  threads=$2
  shift 2
else
  threads=$(nproc)
fi

project_file=$1

# Remove build directory from the compilation database: cppcheck will throw
# an error if any file doesn't exist, even if it's ignored with -i.
filtered_project_file="$(dirname "$project_file")/cppcheck_compile_commands.json"

# $build_dir is /root/project/build
< "$project_file" > "$filtered_project_file" \
  jq '(.[0].directory + "/") as $build_dir |
    [.[] | select(.file | startswith($build_dir) | not)]
  ' \
  || ferr "failed to filter compilation database"

echo "Threads: $threads"
echo "cppcheck version: $(cppcheck --version)"
cppcheck \
  -j $threads \
  --xml --xml-version=2 \
  --project=$filtered_project_file \
  --output-file=cppcheck.xml \
  --std=c++20 \
  --platform=unix64 \
  --error-exitcode=1 \
  -i /root/project/tests/ \
  -i /root/project/3rdParty/ \
  -D__linux__ \
  --inline-suppr \
  --suppress="*:*3rdParty/*" \
  --suppress="*:*3rdParty/immer/immer/detail/rbts/*" \
  --suppress="*:*lib/Basics/Endian.h" \
  --suppress="*:*lib/Basics/fpconv.cpp" \
  --suppress="*:*lib/Basics/short_alloc.h" \
  --suppress="*:*lib/Geo/karney/*" \
  --suppress="*:*lib/iresearch/core/shared.hpp" \
  --suppress="*:*lib/Zip/*" \
  --suppress="preprocessorErrorDirective:*lib/SimpleHttpClient/SslClientConnection.cpp" \
  --suppress="constStatement" \
  --suppress="cppcheckError" \
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
  --suppress="shadowVariable" \
  --suppress="stlFindInsert" \
  --suppress="syntaxError" \
  --suppress="uninitMemberVar" \
  --suppress="unreadVariable" \
  --suppress="useStlAlgorithm" \
  --suppress="variableScope" || ferr "failed to run cppcheck"

