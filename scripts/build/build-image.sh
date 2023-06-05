set -x
docker build -t arangodb/build-alpine-x86_64:latest --file arangodb-build.Dockerfile .

GCC_VERSION=$(docker run --rm -it arangodb/build-alpine-x86_64:latest g++ "--version" | grep -Po " \K\d+\.\d+(?=\.\d+ )")

OPENSSL_VERSION=$(docker run --rm -it arangodb/build-alpine-x86_64:latest sh -c "cat \${OPENSSL_ROOT_DIR}/lib64/pkgconfig/openssl.pc" | grep -Po "Version: \K\d+\.\d+\.\d+")

OS_VERSION=$(docker run --rm -it arangodb/build-alpine-x86_64:latest cat /etc/alpine-release | grep -Po "\d+\.\d+")

IMAGE_TAG=${OS_VERSION}-gcc${GCC_VERSION}-openssl${OPENSSL_VERSION}

echo "Taging image as \"${IMAGE_TAG}\""
docker tag arangodb/build-alpine-x86_64:latest arangodb/build-alpine-x86_64:${IMAGE_TAG}

docker push arangodb/build-alpine-x86_64:${IMAGE_TAG}
