#!/bin/bash
curl -X POST http://localhost:4001/_api/agency/read -d '[["/"]]' | jq .
