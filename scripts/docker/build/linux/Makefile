IMAGE = arangodb/ubuntubuildarangodb-devel
TAG := $(shell git rev-parse --short HEAD)

all: amd64 manifest

amd64:
	docker build -t ${IMAGE}:${TAG}-x86_64 . -f Dockerfile.x86-64 .
	docker push ${IMAGE}:${TAG}-x86_64

manifest:
	./build-manifest.sh ${IMAGE} ${TAG}

arm64:
	docker build -t ${IMAGE}:${TAG}-arm64v8 -f Dockerfile.arm64 .
	docker push ${IMAGE}:${TAG}-arm64v8

