#!/usr/bin/env bash
if [ "$*" == "" ] ; then
    curl -Ls http://localhost:4001/_api/agency/read -d '[["/"]]' | jq .
else
    curl -Ls http://localhost:4001/_api/agency/read -d '[["/"]]' | jq $*
fi
