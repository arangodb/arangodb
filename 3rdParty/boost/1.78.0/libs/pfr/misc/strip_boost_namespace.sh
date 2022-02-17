#!/bin/bash

# Copyright (c) 2021 Antony Polukhin
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

echo "***** Making target path"
TARGET_PATH="`dirname \"$0\"`/../../pfr_non_boost"
rm -rf ${TARGET_PATH}/*
mkdir -p ${TARGET_PATH}
TARGET_PATH=`cd "${TARGET_PATH}";pwd`

SOURCE_PATH="`dirname \"$0\"`/.."
SOURCE_PATH=`cd "${SOURCE_PATH}";pwd`

echo "***** Copying from ${SOURCE_PATH} to ${TARGET_PATH}"
cp -rf ${SOURCE_PATH}/* ${TARGET_PATH}

mv ${TARGET_PATH}/include/boost/* ${TARGET_PATH}/include/
rm -rf ${TARGET_PATH}/include/boost
rm -rf ${TARGET_PATH}/test
rm ${TARGET_PATH}/misc/strip_boost_namespace.sh
rm -rf ${TARGET_PATH}/meta

echo "***** Changing sources"
find ${TARGET_PATH} -type f | xargs sed -i 's|namespace boost { ||g'
find ${TARGET_PATH} -type f | xargs sed -i 's|namespace boost {||g'
find ${TARGET_PATH} -type f | xargs sed -i 's|} // namespace boost::| // namespace |g'

find ${TARGET_PATH} -type f | xargs sed -i 's/::boost::pfr/::pfr/g'
find ${TARGET_PATH} -type f | xargs sed -i 's/boost::pfr/pfr/g'
find ${TARGET_PATH} -type f | xargs sed -i 's/BOOST_PFR_/PFR_/g'
find ${TARGET_PATH} -type f | xargs sed -i 's|boost/pfr|pfr|g'

find ${TARGET_PATH}/doc -type f | xargs sed -i 's|boost.pfr.|pfr.|g'
find ${TARGET_PATH}/doc -type f | xargs sed -i 's|Boost.PFR|PFR|g'

sed -i  's|# \[Boost.PFR\](https://boost.org/libs/pfr)|# [PFR](https://apolukhin.github.io/pfr_non_boost/)|g' ${TARGET_PATH}/README.md

echo -n "***** Testing: "
if g++-9 -std=c++17 -DPFR_USE_LOOPHOLE=0 -DPFR_USE_CPP17=1 -I ${TARGET_PATH}/include/ ${TARGET_PATH}/example/motivating_example0.cpp && ./a.out > /dev/null; then
    echo -n "OK"
else
    echo -n "FAIL"
    exit 2
fi
if g++-9 -std=c++17 -DPFR_USE_LOOPHOLE=1 -DPFR_USE_CPP17=0 -I ${TARGET_PATH}/include/ ${TARGET_PATH}/example/motivating_example0.cpp && ./a.out > /dev/null; then
    echo -n ", OK"
else
    echo -n ", FAIL"
    exit 3
fi

if g++-9 -std=c++17 -DPFR_USE_LOOPHOLE=0 -DPFR_USE_CPP17=0 -I ${TARGET_PATH}/include/ ${TARGET_PATH}/example/get.cpp && ./a.out > /dev/null; then
    echo -e ", OK"
else
    echo -e ", FAIL"
    exit 4
fi

rm a.out || :

echo "***** Done"
