#!/bin/bash

# Work around stoneage curl versions that can't follow redirects

curl -D /tmp/headers -H 'Accept: application/octet-stream' "$1"

for URL in $(grep location: /tmp/headers |sed -e "s;\r;;" -e "s;.*: ;;"); do
    curl $URL > $2
done
