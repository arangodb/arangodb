#!/usr/bin/env bash

## locate executable
clang_format="${ARANGODB_CLANG_FORMAT:-""}"

for candidate in "clang-format-arangodb" "clang-format-6" "clang-format"; do
    path=$(type -p candidate)
    if [[ -n $path ]]; then
        clang_format="$path"
        break;
    fi
done

# fallback if nothing is found
if [[ -z $clang_format ]]; then
    clang_format="clang-format"
fi

## check version
echo "checking version of $clang_format"
version_string=$(${clang_format} --version)
re=".*version 6.0.1.*"
if ! [[ $version_string =~ $re ]]; then
    echo "your version: '$version_string' does not match version 6.0.1"
    exit 1
fi


## find relevant files
files="$(
    find arangod arangosh lib enterprise \
        -name Zip -prune -o \
        -type f "(" -name "*.cpp" -o -name "*.h" ")" \
        "!" "(" -name "tokens.*" -o -name "v8-json.*" -o -name "voc-errors.*" -o -name "grammar.*" -o -name "xxhash.*" -o -name "exitcodes.*" ")"
)"

## do final formatting
${clang_format} -i -verbose ${files[@]}
