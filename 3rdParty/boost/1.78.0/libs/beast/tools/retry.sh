#!/usr/bin/env bash

for i in {1..3}
do
    $1 "${@:2:99}" && exit 0;
    export BEAST_RETRY="true"
done

exit 1
