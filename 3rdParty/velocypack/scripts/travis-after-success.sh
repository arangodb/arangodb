#!/bin/bash
set -u
ferr(){
    echo "$@"
    exit 1
}

#prepare
project_dir="$(readlink -f .)"
build_dir="$project_dir/build"

echo "project directory $project_dir"
echo "build directory $build_dir"
cd ${project_dir} || ferr "can not enter build dir"

if ${CI:-false}; then
  gem install coveralls-lcov || ferr "failed to install gem"
fi

CXX=${CXX:='gcc'}
version=${CXX#*-}
if [[ -n $version ]]; then
    version="-$version"
fi
GCOV=gcov${version}
echo "gcov: $GCOV"

LCOV=(
    'lcov'
    '--directory' "$project_dir"
    '--gcov-tool' "$(type -p ${GCOV})"
)

# clear counters
"${LCOV[@]}" --capture --initial --output-file base_coverage.info || ferr "failed lcov"
"${LCOV[@]}" --zerocounters || ferr "failed lcov"

# run tests
(cd "${build_dir}/tests" && ctest -V) || ferr "failed to run tests"

# collect coverage info
"${LCOV[@]}" --capture --output-file test_coverage.info || ferr "failed lcov"
"${LCOV[@]}" --add-tracefile base_coverage.info --add-tracefile test_coverage.info --output-file coverage.info || ferr "failed lcov"
"${LCOV[@]}" --remove coverage.info \
             '/usr/*' \
             '*CMakeFiles/*' \
             "$project_dir"'/examples/*' \
             "$project_dir"'/tools/*' \
             "$project_dir"'/tests/*' \
             "$project_dir"'/include/velocypack/velocypack-xxhash*' \
             "$project_dir"'/src/*xxh*' \
             "$project_dir"'/src/*hash*' \
             "$project_dir"'/src/powers.h' \
              --output-file coverage.info || ferr "failed lcov"

"${LCOV[@]}" --list coverage.info || ferr "failed lcov"

sed -i "s#${project_dir}/##" coverage.info
# upload coverage info
if ${COVERALLS_TOKEN:-false}; then
  coveralls-lcov --repo-token ${COVERALLS_TOKEN} coverage.info || ferr "failed to upload"
else
  # should not be required on github
  coveralls-lcov coverage.info || ferr "failed to upload"
fi
