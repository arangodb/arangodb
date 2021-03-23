#!/bin/bash
set -u
ferr(){
    echo "$@"
    exit 1
}

(cd build/tests && ctest --output-on-failure) || ferr "failed to run tests"
