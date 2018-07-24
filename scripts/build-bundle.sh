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
    --parallel 8 \
    --package Bundle \
    --buildDir build-${EP}bundle \
    --targetDir /var/tmp/ \
    --downloadStarter \
    --prefix "/opt/arangodb" \
    --clang \
    --staticOpenSSL \
    $@

cd ${DIR}/..
