#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..
EP="y"
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="e"
    fi
done

./Installation/Jenkins/build.sh \
    standard \
    --msvc \
    --buildDir /cygdrive/c/b/${EP}/ \
    --package NSIS \
    --targetDir /var/tmp/ \
    $@

cd ${DIR}/..
