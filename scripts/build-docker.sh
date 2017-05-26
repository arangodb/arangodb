#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOTDIR=${DIR}/..
BUILDDIR=${ROOTDIR}/build 
mkdir -p ${BUILDDIR}
mkdir -p ${ROOTDIR}/build-tmp

VERSION=$(cat ${ROOTDIR}/VERSION)
export DEBVERSION=${VERSION}-1
DOCKERTAG=${VERSION}-local
DEBIMAGE_NAME="arangodb3-${DEBVERSION}_amd64"
BUILDDEB_ARGS=""
for i in $@; do
    if test "$i" == "--enterprise"; then
        DEBIMAGE_NAME="arangodb3e-${DEBVERSION}_amd64"
        BUILDDEB_ARGS="--enterprise xyz"
    fi
done

# Create debian-jessie-builder
if [ ! -e ${BUILDDIR}/.build-docker-containers ]; then
    cd ${BUILDDIR} 
    git clone git@github.com:arangodb-helper/build-docker-containers.git ${BUILDDIR}/.build-docker-containers
fi
docker build -t arangodb/debian-jessie-builder ${BUILDDIR}/.build-docker-containers/distros/debian/jessie/build/

# Ensure arangodb/arangodb-docker exists 
if [ ! -e ${BUILDDIR}/.arangodb-docker ]; then
    cd ${BUILDDIR}
    git clone git@github.com:arangodb/arangodb-docker.git ${BUILDDIR}/.arangodb-docker
fi 

# Build jessie package
docker run -it -v ${ROOTDIR}:/arangodb -v ${ROOTDIR}/build-tmp:/var/tmp arangodb/debian-jessie-builder /arangodb/scripts/build-deb.sh $BUILDDEB_ARGS

# Copy deb image to docker build root
cp -a ${ROOTDIR}/build-tmp/${DEBIMAGE_NAME}.deb ${BUILDDIR}/.arangodb-docker/arangodb.deb

# Build docker image
cd ${BUILDDIR}/.arangodb-docker
docker build -f Dockerfile3.local -t arangodb:${DOCKERTAG} .
cd ${ROOTDIR}