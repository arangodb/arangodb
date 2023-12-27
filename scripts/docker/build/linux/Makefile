IMAGE = arangodb/ubuntubuildarangodb7
TAG = 6

all: amd64 manifest

amd64:
	docker build -t ${IMAGE}-x86_64:${TAG} .
	docker push ${IMAGE}-x86_64:${TAG}

manifest:
	./build-manifest.sh ${IMAGE} ${TAG}

arm64:
	docker build -t ${IMAGE}-aarch64:${TAG} -f Dockerfile.arm64 .
	docker push ${IMAGE}-aarch64:${TAG}


