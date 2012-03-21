#!/bin/bash

find . \
    \( -name 3rdParty -o -name .svn \) -prune \
    -o \
    \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.dox" \) \
    -exec "grep" "^/// @verbinclude" "{}" ";" \
    \
    | sort | uniq