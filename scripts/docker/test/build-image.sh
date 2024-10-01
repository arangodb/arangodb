#!/bin/sh

set -e

image=$1
arch=$2

docker build --platform linux/$arch --build-arg arch=$arch -t $image:latest-$arch --file arangodb-test.Dockerfile .

echo "Built docker image $image:latest-$arch"