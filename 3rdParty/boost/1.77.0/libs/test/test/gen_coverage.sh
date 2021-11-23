#!/bin/sh

which lcov 1>/dev/null 2>&1
if [ $? != 0 ]
then
    echo "You need to have lcov installed in order to generate the test coverage report"
    exit 1
fi

bjam toolset=gcc-4.9.2 clean
bjam toolset=gcc-4.9.2 runtime-param-test

# Generate html report
lcov --base-directory . --directory ../../../bin.v2/libs/test/test/runtime-param-test.test/gcc-4.9.2/debug/utils-ts -c -o runtime-param-test.info
lcov --remove runtime-param-test.info "/usr*" -o runtime-param-test.info # remove output for external libraries
lcov --remove runtime-param-test.info  "boost" "/boost/c*" "/boost/d*"  "/boost/e*"  "/boost/f*"  "/boost/l*" "/boost/m*" "/boost/s*" -o runtime-param-test.info # remove output for other boost libs
rm -rf ./coverage
genhtml -o ./coverage -t "runtime-param-test coverage" --num-spaces 4 runtime-param-test.info

#Clean up
rm *.info