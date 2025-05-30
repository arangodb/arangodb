#!/bin/sh

set -e

image=$1
arch=$2

docker build --platform linux/${arch} --build-arg arch=${arch} -t ${image}-go:latest-${arch} --file Dockerfile .

echo "Built docker image ${image}-go:latest-${arch}"
