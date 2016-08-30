#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

./Installation/Jenkins/build.sh \
    standard \
    --msvc \
    --buildDir /cygdrive/c/b/y/ \
    --package NSIS \
    --targetDir /var/tmp/ \
    $@

cd ${DIR}/..
