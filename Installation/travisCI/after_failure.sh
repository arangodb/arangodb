#!/bin/bash

echo "$0: checking for core file"
if [[ -f core ]]; then
  echo "$0: found a core file"
  echo "thread apply all bt full" | gdb -c core bin/arangod
fi

echo
echo "$0: compiling ArangoDB"

make -j2 || exit 1

echo
echo "$0: testing ArangoDB"

make jslint unittests-shell-server unittests-shell-server-ahuacatl unittests-http-server SKIP_RANGES=1 || exit 1

echo
echo "$0: done"
