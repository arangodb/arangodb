#!/bin/sh

set -e

image=$1
arch=$2
dockerdir=$3

# No layer cache, so apt-get upgrade always runs. Only the base test image pulls;
# the suite images are built FROM the test image built just before them.
pull=""
if [ "${dockerdir}" = "test" ]; then
  pull="--pull"
fi

docker build ${pull} --no-cache --platform linux/${arch} --build-arg arch=${arch} -t ${image}:latest-${arch} ${dockerdir}

echo "Built docker image ${image}:latest-${arch}"
