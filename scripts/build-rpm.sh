#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

EP=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="EP"
    fi
done

if ! test -d 3rdParty/arangodb-starter; then
    MOREOPTS="${MOREOPTS} --downloadStarter"
fi

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --package RPM \
    --buildDir build-${EP}rpm \
    --targetDir /var/tmp/ \
    --jemalloc \
    --noopt \
    ${MOREOPTS} \
    $@

cd ${DIR}/..
