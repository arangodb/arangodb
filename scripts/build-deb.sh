#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --parallel 25 \
    --package DEB \
    --buildDir build-deb \
    --targetDir /var/tmp/ \
    --jemalloc \
    $@

cd ${DIR}/..
