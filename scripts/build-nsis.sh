#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..
EP="y"
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="e"
    fi
done

if ! test -d 3rdParty/arangodb-starter; then
    MOREOPTS="${MOREOPTS} --downloadStarter"
fi

./Installation/Jenkins/build.sh \
    standard \
    --msvc \
    --buildDir /cygdrive/c/b/${EP}/ \
    --package NSIS \
    --targetDir /var/tmp/ \
    ${MOREOPTS} \
    $@

cd ${DIR}/..
