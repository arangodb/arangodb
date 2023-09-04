set -e

manifest=arangodb/test-ubuntu
# IMAGE_TAG is set within build-image_*.sh and expected to be equal there
[ -z "$IMAGE_TAG" ] && echo "IMAGE_TAG env variable is not set!" && exit 1

echo "Creating docker multiarch manifest \"${manifest}:${IMAGE_TAG}\" and \"${manifest}:latest\":"
docker manifest rm ${manifest}:${IMAGE_TAG}
docker manifest rm ${manifest}:latest

docker manifest create ${manifest}:latest \
  --amend ${manifest}-latest-amd64 \
  --amend ${manifest}:latest-arm64 \
&& docker manifest create ${manifest}:${IMAGE_TAG} \
  --amend ${manifest}:${IMAGE_TAG}-amd64 \
  --amend ${manifest}:${IMAGE_TAG}-arm64 \
|| echo "Error during docker multiarch manifest creation!" && exit 1

echo "To push the manifest please run:"
echo "  docker manifest push --purge ${manifest}:${IMAGE_TAG}"
