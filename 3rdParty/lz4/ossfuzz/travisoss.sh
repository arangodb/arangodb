#!/bin/bash

set -ex

# Clone the oss-fuzz repository
git clone https://github.com/google/oss-fuzz.git /tmp/ossfuzz

if [[ ! -d /tmp/ossfuzz/projects/lz4 ]]
then
    echo "Could not find the lz4 project in ossfuzz"
    exit 1
fi

# Modify the oss-fuzz Dockerfile so that we're checking out the current branch on travis.
sed -i "s@https://github.com/lz4/lz4.git@-b $TRAVIS_BRANCH https://github.com/lz4/lz4.git@" /tmp/ossfuzz/projects/lz4/Dockerfile

# Try and build the fuzzers
pushd /tmp/ossfuzz
python infra/helper.py build_image --pull lz4
python infra/helper.py build_fuzzers lz4
popd
