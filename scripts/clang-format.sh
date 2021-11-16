#!/usr/bin/env bash
[[ -z "$1" ]] && adb_dir="${PWD}" || adb_dir="$1" # If no argument is provided, set adb_dir to pwd
docker run -it --rm -v "$adb_dir":/usr/src/arangodb arangodb/clang-format:latest