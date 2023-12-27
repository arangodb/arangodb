set -e

manifest=$1
IMAGE_TAG=$2

[ -z manifest ] && manifest=neunhoef/ubuntubuildarangodb-devel
# IMAGE_TAG is set within build-image_*.sh and expected to be equal there
[ -z "$IMAGE_TAG" ] && IMAGE_TAG=user

echo "Creating docker multiarch manifest \"${manifest}:${IMAGE_TAG}\":"
set +e; docker manifest rm -f ${manifest}:${IMAGE_TAG} 2>/dev/null; set -e
docker manifest create ${manifest}:${IMAGE_TAG} \
  --amend ${manifest}:${IMAGE_TAG}-x86_64 \
  --amend ${manifest}:${IMAGE_TAG}-aarch64 \
|| ( echo "Error during docker multiarch manifest creation!"; exit 1 )

echo "To push the manifest please run:"
echo "  docker manifest push --purge ${manifest}:${IMAGE_TAG}"
