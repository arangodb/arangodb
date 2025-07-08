set -e
set -x
manifest=$1
tag=$2

echo "Creating docker multiarch manifest \"${manifest}:${tag}\":"
docker manifest create ${manifest}:${tag} \
  --amend ${manifest}:${tag}-amd64 \
  --amend ${manifest}:${tag}-arm64 \
|| ( echo "Error during docker multiarch manifest creation!"; exit 1 )

docker manifest push --purge ${manifest}:${tag}
