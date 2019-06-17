#!/usr/bin/env bash

find arangod arangosh lib enterprise \
     -name Zip -prune -o \
     -type f "(" -name "*.cpp" -o -name "*.h" ")" \
     "!" "(" -name "tokens.*" -o -name "v8-json.*" -o -name "voc-errors.*" -o -name "grammar.*" -o -name "xxhash.*" -o -name "exitcodes.*" ")" | \
     xargs clang-format -i -verbose
