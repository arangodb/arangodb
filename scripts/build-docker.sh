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
BUILDDEB_DOCKER_ARGS=""
DOCKERFILENAME=Dockerfile$(echo ${VERSION} | cut -d '.' -f 1,2 --output-delimiter=).local
for i in $@; do
    if test "$i" == "--enterprise"; then
        DEBIMAGE_NAME="arangodb3e-${DEBVERSION}_amd64"
        BUILDDEB_ARGS="--enterprise git@github.com:arangodb/enterprise.git "
    fi
done
if [ ! -z "${SSH_AUTH_SOCK}" ]; then
    BUILDDEB_DOCKER_ARGS="${BUILDDEB_DOCKER_ARGS} -v ${SSH_AUTH_SOCK}:/.ssh-agent -e SSH_AUTH_SOCK=/.ssh-agent"
fi

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
docker run -it \
    -e GIT_SSH_COMMAND="ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
    -v ${ROOTDIR}:/arangodb \
    -v ${ROOTDIR}/build-tmp:/var/tmp \
    -w /arangodb \
    ${BUILDDEB_DOCKER_ARGS} \
    arangodb/debian-jessie-builder \
    /arangodb/scripts/build-deb.sh $BUILDDEB_ARGS

# Copy deb image to docker build root
cp -a ${ROOTDIR}/build-tmp/${DEBIMAGE_NAME}.deb ${BUILDDIR}/.arangodb-docker/arangodb.deb

# Build docker image
cd ${BUILDDIR}/.arangodb-docker
docker build -f ${DOCKERFILENAME} -t arangodb:${DOCKERTAG} .
cd ${ROOTDIR}