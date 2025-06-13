#!/bin/sh

set -e

image=$1
arch=$2
dockerdir=$3

docker build --platform linux/${arch} --build-arg arch=${arch} -t ${image}:latest-${arch} ${dockerdir}

echo "Built docker image ${image}:latest-${arch}"
