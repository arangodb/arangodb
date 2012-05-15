#!/bin/bash

find . \
    \( -name 3rdParty -o -name .svn \) -prune \
    -o \
    \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.dox" \) \
    -exec "grep" '^/// @\(verbinclude \|EXAMPLE{\|TINYEXAMPLE{\)' "{}" ";" \
    | tr "{" " " \
    | tr "," " " \
    | awk '{print $3}' | sort | uniq > /tmp/verbinclude.used

for file in Durham Fyn ArangoDB; do
    (cd Doxygen/Examples.$file && ls -1) | fgrep -v "~"
done | sort | uniq > /tmp/verbinclude.examples

diff /tmp/verbinclude.used /tmp/verbinclude.examples
