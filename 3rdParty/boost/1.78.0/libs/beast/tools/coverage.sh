#! /bin/bash
set -e
lcov --directory bin.v2 --capture --no-external --directory $(pwd) --output-file coverage.info > /dev/null 2>&1
lcov --extract coverage.info $(pwd)'/boost/beast/*' --output-file coverage.info > /dev/null
lcov --remove coverage.info $(pwd)'/boost/beast/_experimental/*' --output-file coverage.info > /dev/null
lcov --list coverage.info
# Codecov improperly detects project root in AzP, so we need to upload from beast git repo
cd libs/beast
curl -s https://codecov.io/bash -o codecov && bash ./codecov -X gcov -f ../../coverage.info -t $CODECOV_TOKEN
