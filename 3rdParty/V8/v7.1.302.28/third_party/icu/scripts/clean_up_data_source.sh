#!/bin/bash

echo "Reverting all the data trimming/customization in source/data ..."
cd "$(dirname "$0")/.."
git checkout -- source/data
