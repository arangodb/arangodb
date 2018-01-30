#!/bin/bash

set -e

BRANCH="$1"
CRYPT="$2"

echo "The name of the docker image will be: \n arangodb:$LOCAL_TAG arangodb/arangodb-preview:enterprise-nightly.$BRANCH-$CRYPT"

if [ -z "$BRANCH" ]; then
    echo "usage: $0 <branch-name>"
    exit 1
fi

./scripts/build-docker.sh --enterprise

LOCAL_TAG="`cat build/.arangodb-docker/.docker-tag`"

if [ -z "$LOCAL_TAG" ]; then
    echo "$0: docker build did not succeed, please check logs"
    echo "$0: missing file 'build/.arangodb-docker/.docker-tag'"
    exit 1
fi

docker tag arangodb:$LOCAL_TAG arangodb/arangodb-preview:enterprise-nightly.$BRANCH-$CRYPT
docker push arangodb/arangodb-preview:enterprise-nightly.$BRANCH-$CRYPT
