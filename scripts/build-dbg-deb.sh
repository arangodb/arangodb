#!/usr/bin/env bash
# To install the build libraries needed by this script run:
# `sudo apt-get install cmake libjemalloc-dev libssl-dev libreadline-dev`
# After the packages are built you will find them in /var/tmp

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

EP=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="EP"
    fi
done

export CPU_CORES=$(grep -c ^processor /proc/cpuinfo)

if ! test -d 3rdParty/arangodb-starter; then
    MOREOPTS="${MOREOPTS} --downloadStarter"
fi

./Installation/Jenkins/build.sh \
    debug \
    --failure-tests \
    --rpath \
    --parallel ${CPU_CORES} \
    --package DEB \
    --buildDir build-${EP}deb-dbg \
    --targetDir /var/tmp/ \
    --noopt \
    ${MOREOPTS} \
    $@

cd ${DIR}/..
