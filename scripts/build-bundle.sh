#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..
EP=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="EP"
    fi
done

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --parallel 5 \
    --package Bundle \
    --buildDir build-${EP}bundle \
    --prefix "/opt/arangodb" \
    --targetDir /var/tmp/ \
    --clang \
    $@

cd ${DIR}/..
