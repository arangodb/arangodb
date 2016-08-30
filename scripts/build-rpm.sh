#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --package RPM \
    --buildDir build-rpm \
    --targetDir /var/tmp/ \
    --jemalloc \
    $@

cd ${DIR}/..
