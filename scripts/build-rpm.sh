#!/bin/sh

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --package RPM \
    --jemalloc \

cd ${DIR}/..
