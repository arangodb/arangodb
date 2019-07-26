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


./Installation/Jenkins/build.sh \
    standard \
    --parallel 25 \
    --package DEB \
    $SNAP \
    --xcArm /usr/bin/arm-linux-gnueabihf \
    --buildDir build-${EP}deb \
    --targetDir /var/tmp/ \
    --noopt \
    $@

cd ${DIR}/..
