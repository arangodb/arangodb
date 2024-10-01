#!/bin/sh

image=$1
arch=$2

OS_VERSION=$(docker run --rm -it $image:latest-$arch cat /etc/os-release | grep -Po "VERSION_ID=\"\K\d+\.\d+")

echo ${OS_VERSION}-$(git rev-parse --short HEAD)

