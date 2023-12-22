set -e

arch=arm64

image=arangodb/build-ubuntu
docker build --platform linux/$arch -t $image:latest-$arch --file Dockerfile.arm64 .

GCC_VERSION=$(docker run --rm -it $image:latest-$arch g++ "--version" | grep -Po " \K\d+\.\d+(?=\.\d+ )")

OPENSSL_VERSION=$(docker run --rm -it $image:latest-$arch sh -c "cat /opt/lib/pkgconfig/openssl.pc" | grep -Po "Version: \K\d+\.\d+\.\d+")

OS_VERSION=23.10

IMAGE_TAG=${OS_VERSION}-gcc${GCC_VERSION}-openssl${OPENSSL_VERSION}-$(git rev-parse --short HEAD)-$arch

echo "Tagging image as \"${IMAGE_TAG}\""
docker tag $image:latest-$arch $image:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push $image:${IMAGE_TAG}"
