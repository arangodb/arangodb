set -e

image=arangodb/test-ubuntu-x86-64
docker build -t $image:latest --file arangodb-test-x86-64.Dockerfile .

OS_VERSION=$(docker run --rm -it $image:latest cat etc/os-release | grep -Po "VERSION_ID=\"\K\d+\.\d+")

IMAGE_TAG=${OS_VERSION}

echo "Taging image as \"${IMAGE_TAG}\""
docker tag $image:latest $image:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push $image:${IMAGE_TAG}"
