#!/bin/bash
#
# This script runs the go-driver tests against an ArangoDB enterprise 
# docker image that has encryption turned on.
#
# The ArangoDB docker image is typically created by `build-docker.sh`.
#
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOTDIR=${DIR}/..
BUILDDIR=${ROOTDIR}/build 
mkdir -p ${BUILDDIR}
ENCRYPTION_KEYFILE=${BUILDDIR}/.encryption-keyfile

VERSION=$(cat ${ROOTDIR}/VERSION)
DOCKERTAG=${VERSION}-local

# Fetch go driver
if [ ! -e ${BUILDDIR}/.go-driver ]; then
    cd ${BUILDDIR} 
    git clone git@github.com:arangodb/go-driver.git ${BUILDDIR}/.go-driver
fi

# Start test 
if [ ! -e ${ENCRYPTION_KEYFILE} ]; then
    dd if=/dev/random bs=1 count=32 of=${ENCRYPTION_KEYFILE}
fi
export ARANGODB="-v ${ENCRYPTION_KEYFILE}:/encryption-keyfile:ro arangodb:${DOCKERTAG} --server.storage-engine=rocksdb --rocksdb.encryption-keyfile=/encryption-keyfile"
make -C ${BUILDDIR}/.go-driver run-tests-single
