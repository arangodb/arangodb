#! /bin/bash
#
# Copyright 2017 - 2019 James E. King III
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
#      http://www.boost.org/LICENSE_1_0.txt)
#
# Bash script to run in travis to perform codecov.io integration
#

# assumes cwd is the top level directory of the boost project
# assumes an environment variable $SELF is the boost project name

set -ex

. ci/enforce.sh

if [ -z "$GCOV" ]; then
    if [ "${B2_TOOLSET%%-*}" == "gcc" ]; then
        ver="${B2_TOOLSET#*-}"
        GCOV=gcov-${ver}
    else
        GCOV=gcov-7 # default
    fi
fi

# lcov after 1.14 needs this
# sudo apt install --no-install-recommends -y libperlio-gzip-perl libjson-perl

# install the latest lcov we know works
# some older .travis.yml files install the tip which may be unstable
rm -rf /tmp/lcov
pushd /tmp
git clone -b v1.14 https://github.com/linux-test-project/lcov.git
export PATH=/tmp/lcov/bin:$PATH
which lcov
lcov --version
popd

B2_VARIANT=variant=debug
ci/travis/build.sh cxxflags=--coverage linkflags=--coverage 
#cxxflags=-fprofile-arcs cxxflags=-ftest-coverage linkflags=-fprofile-arcs linkflags=-ftest-coverage

# switch back to the original source code directory
cd $TRAVIS_BUILD_DIR

# coverage files are in ../../b2 from this location
lcov --gcov-tool=$GCOV --rc lcov_branch_coverage=0 --base-directory "$BOOST_ROOT/libs/$SELF" --directory "$BOOST_ROOT" --capture --output-file all.info

# all.info contains all the coverage info for all projects - limit to ours
# first we extract the interesting headers for our project then we use that list to extract the right things
for f in `for f in include/boost/*; do echo $f; done | cut -f2- -d/`; do echo "*/$f*"; done > /tmp/interesting
echo headers that matter:
cat /tmp/interesting
xargs -L 999999 -a /tmp/interesting lcov --gcov-tool=$GCOV --rc lcov_branch_coverage=1 --extract all.info {} "*/libs/$SELF/src/*" --output-file coverage.info

# dump a summary on the console - helps us identify problems in pathing
lcov --gcov-tool=$GCOV --rc lcov_branch_coverage=1 --list coverage.info

#
# upload to codecov.io
#
curl -s https://codecov.io/bash > .codecov
chmod +x .codecov
./.codecov -f coverage.info -X gcov -x "$GCOV"