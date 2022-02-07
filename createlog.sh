#!/bin/bash

curl -s -XPOST http://localhost:8530/_api/log -d '{"id":12, "config":{"waitForSync":false, "writeConcern": 2, "softWriteConcern": 3, "replicationFactor": 3}}' | jq
