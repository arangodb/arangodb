#!/usr/bin/env bash

while read file; do
    echo "$file: $1 -> $2"
    sed -i 's/'$1'/'$2'/' "$file"
done < <(rgrep -l  "$1" * )
