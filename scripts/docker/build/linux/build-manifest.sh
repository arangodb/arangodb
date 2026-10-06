#!/bin/bash
set -e

manifest=$1
IMAGE_TAG=$2

[ -z "$manifest" ] && { echo "Usage: $0 <image> <tag>"; exit 1; }
[ -z "$IMAGE_TAG" ] && IMAGE_TAG=$(git rev-parse --short HEAD)

echo "Creating docker multiarch manifest \"${manifest}:${IMAGE_TAG}\":"
docker manifest rm "${manifest}:${IMAGE_TAG}" 2>/dev/null || true
docker manifest create "${manifest}:${IMAGE_TAG}" \
  --amend "${manifest}:${IMAGE_TAG}-amd64" \
  --amend "${manifest}:${IMAGE_TAG}-arm64v8" \
  || { echo "Error during docker multiarch manifest creation!"; exit 1; }

docker manifest push --purge "${manifest}:${IMAGE_TAG}"
