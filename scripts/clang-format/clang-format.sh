#!/bin/sh
cd ${PWD}/scripts/clang-format # assume current dir is arangodb (not error-proof)
docker build -t clang-format .
while [[ $PWD != '/' && ${PWD##*/} != 'arangodb' ]]; do cd ..; done # cd up to arangodb
docker run -v ${PWD}:/arangodb -ti clang-format:latest