#!/bin/bash
# Determines the tag for the Ubuntu build Docker image from the OS version
# embedded in the built image and the current git commit SHA.
#
# Usage: determine-tag.sh <image> <arch_suffix>
#   image:        full image ref, e.g. public.ecr.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-312
#   arch_suffix:  amd64 | arm64v8

set -e
image=$1
arch_suffix=$2

OS_VERSION=$(docker run --rm -i "${image}:latest-${arch_suffix}" \
  sh -c 'grep VERSION_ID /etc/os-release' | grep -Po 'VERSION_ID="\K[^"]+')

echo "${OS_VERSION}-$(git rev-parse --short HEAD)"
