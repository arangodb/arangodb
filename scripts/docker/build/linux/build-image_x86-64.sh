set -e

arch=amd64

image=arangodb/build-alpine
docker build --platform linux/$arch -t $image:latest-$arch --file arangodb-build.Dockerfile .

GCC_VERSION=$(docker run --rm -it $image:latest-$arch g++ "--version" | grep -Po " \K\d+\.\d+(?=\.\d+ )")

OPENSSL_VERSION=$(docker run --rm -it $image:latest-$arch sh -c "cat \${OPENSSL_ROOT_DIR}/lib64/pkgconfig/openssl.pc" | grep -Po "Version: \K\d+\.\d+\.\d+")

OS_VERSION=$(docker run --rm -it $image:latest-$arch cat /etc/alpine-release | grep -Po "\d+\.\d+")

IMAGE_TAG=${OS_VERSION}-gcc${GCC_VERSION}-openssl${OPENSSL_VERSION}-$(git rev-parse --short HEAD)-$arch

echo "Tagging image as \"${IMAGE_TAG}\""
docker tag $image:latest-$arch $image:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push $image:${IMAGE_TAG}"
