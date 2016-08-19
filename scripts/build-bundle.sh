#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --parallel 5 \
    --package Bundle \
    --buildDir build-bundle \
    --targetDir /var/tmp/ \
    --clang

cd ${DIR}/..
