
#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export CPU_CORES=$(grep -c ^processor /proc/cpuinfo)


cd ${DIR}/..

EP=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        EP="EP"
    fi
done


./Installation/Jenkins/build.sh \
    standard \
    --parallel ${CPU_CORES} \
    --package DEB \
    $SNAP \
    --xcArm /usr/bin/aarch64-linux-gnu \
    --buildDir build-${EP}deb \
    --targetDir /var/tmp/ \
    --noopt \
    $@

cd ${DIR}/..
