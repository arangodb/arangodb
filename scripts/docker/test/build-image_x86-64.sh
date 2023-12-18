set -e

arch=amd64

image=arangodb/test-ubuntu
docker build --platform linux/$arch --build-arg arch=$arch -t $image:latest-$arch --file arangodb-test.Dockerfile .

OS_VERSION=$(docker run --rm -it $image:latest-$arch cat /etc/os-release | grep -Po "VERSION_ID=\"\K\d+\.\d+")

IMAGE_TAG=${OS_VERSION}-$(git rev-parse --short HEAD)-$arch

echo "Tagging image as \"${IMAGE_TAG}\""
docker tag $image:latest-$arch $image:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push $image:${IMAGE_TAG}"
