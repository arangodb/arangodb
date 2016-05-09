#!/bin/bash
if [ "$*" == "" ] ; then
    curl -s X POST http://localhost:4001/_api/agency/read -d '[["/"]]' | jq .
else
    curl -s -X POST http://localhost:4001/_api/agency/read -d '[["/"]]' | jq $*
fi
