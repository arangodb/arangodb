set -e

arch=amd64

image=arangodb/build-ubuntu
docker build --platform linux/$arch -t $image:latest-$arch --file Dockerfile .

GCC_VERSION=$(docker run --rm -it $image:latest-$arch g++ "--version" | head -1 | cut -d " " -f 4 | tr -d "\n\r")

CLANG_VERSION=$(docker run --rm -it $image:latest-$arch clang++ "--version" | head -1 | cut -d " " -f 4 | tr -d "\n\r")

OPENSSL_VERSION=$(docker run --rm -it $image:latest-$arch sh -c "cat /opt/lib64/pkgconfig/openssl.pc" | grep -Po "Version: \K\d+\.\d+\.\d+")

OS_VERSION=23.10

IMAGE_TAG="${OS_VERSION}-clang${CLANG_VERSION}-gcc${GCC_VERSION}-openssl${OPENSSL_VERSION}-$(git rev-parse --short HEAD)-$arch"

echo "Tagging image as \"${IMAGE_TAG}\""
docker tag $image:latest-$arch $image:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push $image:${IMAGE_TAG}"
