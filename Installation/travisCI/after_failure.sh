#!/bin/bash

echo "$0: printing ulimit"
ulimit -a

echo "$0: checking for core file"
COREFILE=$(find . -maxdepth 1 -name "core*" | head -n 1)

if [[ -f "$COREFILE" ]]; then 
  echo "$0: found a core file"
  gdb -c "$COREFILE" bin/arangod -ex "thread apply all bt" -ex "set pagination 0" -batch; 
fi

echo "$0: done"
