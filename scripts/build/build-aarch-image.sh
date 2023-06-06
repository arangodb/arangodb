docker build -t arangodb/build-alpine-aarch64:latest --file arangodb-build-aarch64.Dockerfile .

GCC_VERSION=$(docker run --rm -it arangodb/build-alpine-aarch64:latest g++ "--version" | grep -oEi "([0-9]+\.[0-9]+)" |head -n 1)

OPENSSL_VERSION=$(docker run --rm -it arangodb/build-alpine-aarch64:latest sh -c "cat \${OPENSSL_ROOT_DIR}/lib/pkgconfig/openssl.pc" |grep -oEi " ([0-9]+\.[0-9]+)"| tr -d ' ' |head -n 1)

OS_VERSION=$(docker run --rm -it arangodb/build-alpine-aarch64:latest cat /etc/alpine-release |grep -oEi "([0-9]+\.[0-9]+)"|head -n 1)

IMAGE_TAG=${OS_VERSION}-gcc${GCC_VERSION}-openssl${OPENSSL_VERSION}

echo "Taging image as \"${IMAGE_TAG}\""
docker tag arangodb/build-alpine-aarch64:latest arangodb/build-alpine-aarch64:${IMAGE_TAG}

echo "To push the image please run:"
echo "  docker push arangodb/build-alpine-aarch64:${IMAGE_TAG}"
