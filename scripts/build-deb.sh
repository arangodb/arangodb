#!/bin/bash
# To install the build libraries needed by this script run:
# `sudo apt-get install cmake libjemalloc-dev libssl-dev libreadline-dev`
# After the packages are built you will find them in /var/tmp

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

BUILD_CONFIGURATION="standard"

EP=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="EP"
    fi
    if test "$i" == "--maintainer"; then
        BUILD_CONFIGURATION="maintainer"
    fi
done

export CPU_CORES=$(grep -c ^processor /proc/cpuinfo)

./Installation/Jenkins/build.sh \
    ${BUILD_CONFIGURATION} \
    --rpath \
    --parallel ${CPU_CORES} \
    --package DEB \
    --buildDir build-${EP}deb \
    --targetDir /var/tmp/ \
    --jemalloc \
    --downloadStarter \
    --noopt \
    $@

cd ${DIR}/..
