set -e

image=arangodb/build-alpine-x86_64
docker build -t $image:latest --file arangodb-build-x86-64.Dockerfile .

GCC_VERSION=$(docker run --rm -it $image:latest g++ "--version" | grep -Po " \K\d+\.\d+(?=\.\d+ )")

OPENSSL_VERSION=$(docker run --rm -it $image:latest sh -c "cat \${OPENSSL_ROOT_DIR}/lib64/pkgconfig/openssl.pc" | grep -Po "Version: \K\d+\.\d+\.\d+")

OS_VERSION=$(docker run --rm -it $image:latest cat /etc/alpine-release | grep -Po "\d+\.\d+")

IMAGE_TAG=${OS_VERSION}-gcc${GCC_VERSION}-openssl${OPENSSL_VERSION}

echo "Taging image as \"${IMAGE_TAG}\""
docker tag $image:latest $image:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push $image:${IMAGE_TAG}"
