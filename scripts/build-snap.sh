
#!/usr/bin/env bash

set -e

if test -z ${CXX}; then
  export CC=gcc
  export CXX=g++
fi

EP=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="EP"
    fi
done

export CPU_CORES=$(grep -c ^processor /proc/cpuinfo)

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

if ! test -d 3rdParty/arangodb-starter; then
    MOREOPTS="${MOREOPTS} --downloadStarter"
fi

./Installation/Jenkins/build.sh \
    standard \
    --rpath \
    --parallel ${CPU_CORES} \
    --package TGZ \
    --snap \
    --buildDir build-${EP}snap \
    --targetDir /var/tmp/ \
    --noopt \
    ${MOREOPTS} \
    $@

cd ${DIR}/..
